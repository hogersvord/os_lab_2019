#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h" // ЗАДАНИЕ 3: общая библиотека
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/time.h>  // ИЗМЕНЕНО: таймаут чтения TCP
#include <sys/types.h>

#include <pthread.h>

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
};



uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1 % args->mod;

  // Empty range: multiplication identity.
  if (args->begin > args->end)
    return ans;

  for (uint64_t i = args->begin;; i++) {
    ans = MultModulo(ans, i, args->mod);
    if (i == args->end)
      break;
  }

  return ans;
}

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  fargs->result = Factorial(fargs);
  return NULL;
}

int main(int argc, char **argv) {
  int tnum = -1;
  int port = -1;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      errno = 0;
      char *endptr = NULL;
      long value = strtol(optarg, &endptr, 10);
      if (errno != 0 || endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Invalid number: %s\n", optarg);
        return 1;
      }
      switch (option_index) {
      case 0:
        if (value < 1 || value > 65535) {
          fprintf(stderr, "Port must be between 1 and 65535\n");
          return 1;
        }
        port = (int)value;
        break;
      case 1:
        if (value < 1 || value > 256) {
          fprintf(stderr, "Thread count must be between 1 and 256\n");
          return 1;
        }
        tnum = (int)value;
        break;
      default:
        fprintf(stderr, "Index %d is out of options\n", option_index);
        return 1;
      }
    } break;

    case '?':
      fprintf(stderr, "Unknown argument\n");
      return 1;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
      return 1;
    }
  }

  if (port == -1 || tnum == -1 || optind != argc) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in server = {0};
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val,
                 sizeof(opt_val)) < 0) {
    perror("setsockopt");
    close(server_fd);
    return 1;
  }

  int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
  if (err < 0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  err = listen(server_fd, 128);
  if (err < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  printf("Server listening at %d\n", port);
  fflush(stdout);

  while (true) {
    struct sockaddr_in client = {0};
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);

    if (client_fd < 0) {
      if (errno == EINTR)
        continue;
      perror("accept");
      continue;
    }

    // ИЗМЕНЕНО: не ждём бесконечно неполный посторонний TCP-запрос.
    struct timeval receive_timeout = {.tv_sec = 3, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
               &receive_timeout, sizeof(receive_timeout));

    // ИЗМЕНЕНО: одно TCP-подключение — один запрос факториала.
    do {
      // ИЗМЕНЕНО: ожидаем сигнатуру FACT и три uint64_t.
      char from_client[4 + sizeof(uint64_t) * 3];
      size_t received = 0;
      bool receive_error = false;

      // TCP is a byte stream: recv() may read fewer than 24 bytes.
      while (received < sizeof(from_client)) {
        ssize_t n = recv(client_fd, from_client + received,
                         sizeof(from_client) - received, 0);
        if (n == 0)
          break;
        if (n < 0) {
          if (errno == EINTR)
            continue;
          perror("recv");
          receive_error = true;
          break;
        }
        received += (size_t)n;
      }

      if (receive_error || received == 0)
        break;
      if (received != sizeof(from_client)) {
        fprintf(stderr, "Client sent an incomplete request\n");
        break;
      }

      // ИЗМЕНЕНО: HTTP-запросы, поступающие на перенаправленный порт,
      // не должны интерпретироваться как огромный диапазон факториала.
      if (memcmp(from_client, "FACT", 4) != 0) {
        fprintf(stderr, "Ignored non-factorial request (for example HTTP)\n");
        break;
      }

      uint64_t begin = 0;
      uint64_t end = 0;
      uint64_t mod = 0;
      // ИЗМЕНЕНО: данные начинаются после четырёх байт FACT.
      memcpy(&begin, from_client + 4, sizeof(uint64_t));
      memcpy(&end, from_client + 4 + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 4 + 2 * sizeof(uint64_t), sizeof(uint64_t));

      if (mod == 0 || begin == 0) {
        fprintf(stderr, "Invalid request: mod and begin must be positive\n");
        break;
      }

      printf("Receive: %llu %llu %llu\n",
             (unsigned long long)begin, (unsigned long long)end,
             (unsigned long long)mod);

      pthread_t threads[tnum];
      struct FactorialArgs args[tnum];

      // Distribute the requested range evenly among threads.
      uint64_t count = end >= begin ? end - begin + 1 : 0;
      uint64_t base = count / (uint64_t)tnum;
      uint64_t remainder = count % (uint64_t)tnum;
      uint64_t next = begin;
      int created = 0;

      for (int i = 0; i < tnum; i++) {
        uint64_t size = base + ((uint64_t)i < remainder);
        args[i].begin = size ? next : 1;
        args[i].end = size ? next + size - 1 : 0;
        args[i].mod = mod;
        args[i].result = 1 % mod;

        if (size)
          next += size;

        int thread_err = pthread_create(&threads[i], NULL, ThreadFactorial,
                                        (void *)&args[i]);
        if (thread_err != 0) {
          fprintf(stderr, "pthread_create: %s\n", strerror(thread_err));
          break;
        }
        created++;
      }

      uint64_t total = 1 % mod;
      bool join_ok = true;
      for (int i = 0; i < created; i++) {
        int thread_err = pthread_join(threads[i], NULL);
        if (thread_err != 0) {
          fprintf(stderr, "pthread_join: %s\n", strerror(thread_err));
          join_ok = false;
        } else {
          total = MultModulo(total, args[i].result, mod);
        }
      }

      if (created != tnum || !join_ok) {
        fprintf(stderr, "Could not complete the request\n");
        break;
      }

      printf("Total: %llu\n", (unsigned long long)total);
      char buffer[sizeof(total)];
      memcpy(buffer, &total, sizeof(total));

      size_t sent = 0;
      bool send_ok = true;
      while (sent < sizeof(buffer)) {
        ssize_t n = send(client_fd, buffer + sent,
                         sizeof(buffer) - sent, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0) {
          perror("send");
          send_ok = false;
          break;
        }
        sent += (size_t)n;
      }
      if (!send_ok)
        break;
    } while (false);

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
