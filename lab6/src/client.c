#include <stdbool.h>
#include <stdint.h>  // ИЗМЕНЕНО: тип uint64_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h" // ЗАДАНИЕ 3: общая библиотека
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>  // ИЗМЕНЕНО: inet_pton для IPv4
#include <pthread.h>    // ИЗМЕНЕНО: потоки для параллельной работы с серверами
#include <sys/socket.h>
#include <sys/types.h>

struct Server {
  char ip[255];
  int port;
};

// ИЗМЕНЕНО: каждому потоку передаём свой сервер, диапазон и место для ответа.
struct ServerTask {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t answer;
  bool success;
};



bool ConvertStringToUI64(const char *str, uint64_t *val) {
  // ИЗМЕНЕНО: проверяем строку целиком и сбрасываем errno до преобразования.
  if (!str || !*str || *str == '-')
    return false;

  errno = 0;
  char *end = NULL;
  unsigned long long value = strtoull(str, &end, 10);
  if (errno != 0 || end == str || *end != '\0')
    return false;

  *val = (uint64_t)value;
  return true;
}

// ИЗМЕНЕНО: функция одного потока клиента. Каждый поток обращается
// к СВОЕМУ серверу; исходные send/recv перенесены сюда из цикла main().
void *ContactServer(void *arg) {
  struct ServerTask *task = (struct ServerTask *)arg;
  task->success = false;

  struct sockaddr_in server = {0};
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)task->server.port);
  if (inet_pton(AF_INET, task->server.ip, &server.sin_addr) != 1) {
    fprintf(stderr, "Invalid IP: %s\n", task->server.ip);
    return NULL;
  }

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    perror("socket");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed: %s\n",
            task->server.ip, task->server.port, strerror(errno));
    close(sck);
    return NULL;
  }

  // ИЗМЕНЕНО: сигнатура FACT защищает сервер от HTTP-запросов Codespaces.
  // Числа по-прежнему передаются в порядке байтов этой машины.
  char request[4 + sizeof(uint64_t) * 3];
  memcpy(request, "FACT", 4);
  memcpy(request + 4, &task->begin, sizeof(uint64_t));
  memcpy(request + 4 + sizeof(uint64_t), &task->end, sizeof(uint64_t));
  memcpy(request + 4 + 2 * sizeof(uint64_t), &task->mod, sizeof(uint64_t));

  // ИЗМЕНЕНО: TCP может отправить только часть сообщения за один send().
  size_t sent = 0;
  while (sent < sizeof(request)) {
    ssize_t n = send(sck, request + sent, sizeof(request) - sent,
                     MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      fprintf(stderr, "Send to %s:%d failed\n",
              task->server.ip, task->server.port);
      close(sck);
      return NULL;
    }
    sent += (size_t)n;
  }

  // ИЗМЕНЕНО: recv() тоже может получить ответ по частям.
  char response[sizeof(uint64_t)];
  size_t received = 0;
  while (received < sizeof(response)) {
    ssize_t n = recv(sck, response + received,
                     sizeof(response) - received, 0);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      fprintf(stderr, "Receive from %s:%d failed\n",
              task->server.ip, task->server.port);
      close(sck);
      return NULL;
    }
    received += (size_t)n;
  }

  memcpy(&task->answer, response, sizeof(uint64_t));
  task->success = true;
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  uint64_t mod = 0;
  bool have_k = false, have_mod = false;  // ИЗМЕНЕНО: отличаем отсутствие от 0.
  char servers[255] = {'\0'}; // Путь к файлу (255 — размер буфера пути).

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 0:
      switch (option_index) {
      case 0:
        // ИЗМЕНЕНО: обработка TODO для --k.
        have_k = ConvertStringToUI64(optarg, &k);
        if (!have_k) {
          fprintf(stderr, "Invalid k\n");
          return 1;
        }
        break;
      case 1:
        // ИЗМЕНЕНО: обработка TODO для --mod.
        have_mod = ConvertStringToUI64(optarg, &mod);
        if (!have_mod || mod == 0) {
          fprintf(stderr, "Invalid mod: must be positive\n");
          return 1;
        }
        break;
      case 2:
        // ИЗМЕНЕНО: копируем путь с проверкой длины (без переполнения).
        if (strlen(optarg) >= sizeof(servers)) {
          fprintf(stderr, "Servers path is too long\n");
          return 1;
        }
        strcpy(servers, optarg);
        break;
      default:
        fprintf(stderr, "Unknown option index %d\n", option_index);
        return 1;
      }
      break;
    case '?':
      fprintf(stderr, "Arguments error\n");
      return 1;
    default:
      return 1;
    }
  }

  if (!have_k || !have_mod || !servers[0] || optind != argc) {
    fprintf(stderr, "Using: %s --k 10 --mod 7 --servers servers.txt\n",
            argv[0]);
    return 1;
  }

  // ИЗМЕНЕНО: вместо одного жёстко заданного сервера читаем servers.txt.
  FILE *file = fopen(servers, "r");
  if (!file) {
    perror("fopen servers");
    return 1;
  }

  struct Server *to = NULL;
  size_t servers_num = 0;
  char line[512];

  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '\n' || line[0] == '#')
      continue;

    struct Server entry = {0};
    char extra;
    struct in_addr addr;

    // Формат строки: 127.0.0.1:20001
    if (sscanf(line, " %254[^:]:%d %c", entry.ip,
               &entry.port, &extra) != 2 ||
        entry.port < 1 || entry.port > 65535 ||
        inet_pton(AF_INET, entry.ip, &addr) != 1) {
      fprintf(stderr, "Invalid server entry: %s", line);
      free(to);
      fclose(file);
      return 1;
    }

    struct Server *tmp = realloc(to, (servers_num + 1) * sizeof(*to));
    if (!tmp) {
      perror("realloc");
      free(to);
      fclose(file);
      return 1;
    }
    to = tmp;
    to[servers_num++] = entry;
  }

  if (ferror(file)) {
    perror("read servers");
    free(to);
    fclose(file);
    return 1;
  }
  fclose(file);

  if (servers_num == 0) {
    fprintf(stderr, "Servers file is empty\n");
    free(to);
    return 1;
  }

  // ИЗМЕНЕНО: готовим диапазоны и потоки для ВСЕХ серверов.
  pthread_t *threads = calloc(servers_num, sizeof(*threads));
  struct ServerTask *tasks = calloc(servers_num, sizeof(*tasks));
  if (!threads || !tasks) {
    perror("calloc");
    free(threads);
    free(tasks);
    free(to);
    return 1;
  }

  uint64_t base = k / servers_num;
  uint64_t remainder = k % servers_num;
  uint64_t next = 1;
  size_t created = 0;

  // ИЗМЕНЕНО: запускаем потоки в цикле, не дожидаясь предыдущего.
  for (size_t i = 0; i < servers_num; i++) {
    uint64_t count = base + (i < remainder);
    tasks[i].server = to[i];
    tasks[i].mod = mod;

    if (count > 0) {
      tasks[i].begin = next;
      tasks[i].end = next + count - 1;
      if (i + 1 < servers_num)
        next += count;
    } else {
      // Пустой диапазон: сервер вернёт нейтральный множитель 1 mod mod.
      tasks[i].begin = 1;
      tasks[i].end = 0;
    }

    printf("Server %zu (%s:%d): [%llu, %llu]\n", i + 1,
           to[i].ip, to[i].port,
           (unsigned long long)tasks[i].begin,
           (unsigned long long)tasks[i].end);

    int err = pthread_create(&threads[i], NULL, ContactServer, &tasks[i]);
    if (err != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(err));
      break;
    }
    created++;
  }

  // ИЗМЕНЕНО: ждём ответы и объединяем частичные произведения по модулю.
  uint64_t answer = 1 % mod;
  bool ok = (created == servers_num);
  for (size_t i = 0; i < created; i++) {
    int err = pthread_join(threads[i], NULL);
    if (err != 0 || !tasks[i].success) {
      fprintf(stderr, "Server %zu failed\n", i + 1);
      ok = false;
      continue;
    }
    printf("Server %zu returned: %llu\n", i + 1,
           (unsigned long long)tasks[i].answer);
    answer = MultModulo(answer, tasks[i].answer, mod);
  }

  if (ok)
    printf("answer: %llu\n", (unsigned long long)answer);
  else
    fprintf(stderr, "Calculation failed\n");

  free(threads);
  free(tasks);
  free(to);
  return ok ? 0 : 1;
}
