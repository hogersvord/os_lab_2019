#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

static volatile sig_atomic_t timeout_reached = 0;

void AlarmHandler(int signo)
{
  (void)signo;
  timeout_reached = 1;
}

int main(int argc, char **argv)
{
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = 0;
  bool with_files = false;

  while (true)
  {
    

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
    case 0:
      switch (option_index)
      {
      case 0:
        seed = atoi(optarg);
        if (seed < 0)
        {
          printf("seed must be positive\n");
          return 1;
        }
        break;

      case 1:
        array_size = atoi(optarg);
        if (array_size <= 0)
        {
          printf("array_size must be positive\n");
          return 1;
        }
        break;

      case 2:
        pnum = atoi(optarg);
        if (pnum <= 0)
        {
          printf("pnum must be positive\n");
          return 1;
        }
        break;

      case 3:
        with_files = true;
        break;
      case 4:
      {
        char *endptr;
        errno = 0;

        long value = strtol(optarg, &endptr, 10);

        if (errno != 0 || endptr == optarg ||
            *endptr != '\0' || value <= 0 ||
            value > INT_MAX)
        {
          printf("timeout must be a positive integer\n");
          return 1;
        }

        timeout = (int)value;
        break;
      }
      default:
        printf("Index %d is out of options\n", option_index);
      }
      break;

    case 'f':
      with_files = true;
      break;

    case '?':
      break;

    default:
      printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc)
  {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1)
  {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }

  if (pnum > array_size)
  {
    printf("pnum must not be greater than array_size\n");
    return 1;
  }

int *array = malloc(sizeof(int) * array_size);
pid_t *child_pids = calloc(pnum, sizeof(pid_t));

if (array == NULL || child_pids == NULL) {
    printf("Memory allocation failed\n");
    free(array);
    free(child_pids);
    return 1;
}

GenerateArray(array, array_size, seed);

  /* Для задания 3 */
  int (*pipes)[2] = NULL;

  if (!with_files)
  {
    pipes = malloc(sizeof(int[2]) * pnum);

    for (int i = 0; i < pnum; i++)
    {
      if (pipe(pipes[i]) == -1)
      {
        printf("Pipe failed!\n");
        return 1;
      }
    }
  }

  int active_child_processes = 0;
  int launched_count = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  if (timeout > 0) {
    if (signal(SIGALRM, AlarmHandler) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    alarm(timeout);
}

  for (int i = 0; i < pnum && !timeout_reached; i++) 
  {
    pid_t child_pid = fork();

    if (child_pid >= 0)
    {
      // successful fork

      if (child_pid == 0)
      {
        // child process
        

        int begin = i * array_size / pnum;
        int end = (i + 1) * array_size / pnum;

        struct MinMax local_min_max = GetMinMax(array, begin, end);

        if (with_files)
        {
          // use files here

          char filename[64];
          sprintf(filename, "result_%d.txt", i);

          FILE *file = fopen(filename, "w");

          fprintf(file, "%d %d\n",
                  local_min_max.min,
                  local_min_max.max);

          fclose(file);
        }
        else
        {
          // use pipe here

          close(pipes[i][0]);

          write(pipes[i][1],
                &local_min_max,
                sizeof(local_min_max));

          close(pipes[i][1]);
        }

        free(array);
        return 0;
      }

      child_pids[i] = child_pid;
      launched_count++;
      active_child_processes += 1;

      if (!with_files) {
        close(pipes[i][1]);
      }
    }
    else
    {
      printf("Fork failed!\n");
      return 1;
    }
  }

 bool wait_error = false;

while (active_child_processes > 0 && !timeout_reached) {
    int status;

    pid_t finished = waitpid(
        -1,
        &status,
        timeout > 0 ? WNOHANG : 0
    );

    if (finished > 0) {
        active_child_processes--;

        for (int i = 0; i < launched_count; i++) {
            if (child_pids[i] == finished) {
                child_pids[i] = 0;
                break;
            }
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            wait_error = true;
            break;
        }
    }
    else if (finished == 0) {
        struct timespec delay = {0, 10000000L};
        nanosleep(&delay, NULL);
    }
    else if (errno != EINTR) {
        perror("waitpid");
        wait_error = true;
        break;
    }
}

if (timeout > 0) {
    alarm(0);
}

if (timeout_reached || wait_error) {
    if (timeout_reached) {
        printf("Timeout reached! Killing child processes.\n");
    }

    for (int i = 0; i < launched_count; i++) {
        if (child_pids[i] > 0) {
            if (kill(child_pids[i], SIGKILL) == -1 &&
                errno != ESRCH) {
                perror("kill");
            }
        }
    }

    for (int i = 0; i < launched_count; i++) {
        if (child_pids[i] > 0) {
            while (waitpid(child_pids[i], NULL, 0) == -1 &&
                   errno == EINTR) {
            }
        }
    }

    if (with_files) {
        for (int i = 0; i < launched_count; i++) {
            char filename[64];
            sprintf(filename, "result_%d.txt", i);
            remove(filename);
        }
    } else {
        for (int i = 0; i < pnum; i++) {
            close(pipes[i][0]);
            if (i >= launched_count) {
                close(pipes[i][1]);
            }
        }
    }

    free(pipes);
    free(array);
    free(child_pids);

    return timeout_reached ? 124 : 1;
}

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++)
  {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files)
    {
      // read from files

      char filename[64];
      sprintf(filename, "result_%d.txt", i);

      FILE *file = fopen(filename, "r");

      fscanf(file, "%d %d", &min, &max);

      fclose(file);
      remove(filename);
    }
    else
    {
      // read from pipes

      struct MinMax local_min_max;

      read(pipes[i][0],
           &local_min_max,
           sizeof(local_min_max));

      close(pipes[i][0]);

      min = local_min_max.min;
      max = local_min_max.max;
    }

    if (min < min_max.min)
      min_max.min = min;
    if (max > min_max.max)
      min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  if (pipes != NULL)
  {
    free(pipes);
  }

  free(array);
  free(child_pids);
  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);

  fflush(NULL);
  return 0;
}