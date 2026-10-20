#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>

#include "utils.h"
#include "sum.h"

struct SumArgs
{
  int *array;
  size_t begin;
  size_t end;
  int64_t result;
};

void *ThreadSum(void *args)
{
  struct SumArgs *sum_args = (struct SumArgs *)args;

  sum_args->result = Sum(
      sum_args->array,
      sum_args->begin,
      sum_args->end);

  return NULL;
}

int main(int argc, char **argv)
{

  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;
  int seed_given = 0;

  static struct option options[] = {
      {"threads_num", required_argument, 0, 't'},
      {"seed", required_argument, 0, 's'},
      {"array_size", required_argument, 0, 'a'},
      {0, 0, 0, 0}};

  int option;

  while ((option = getopt_long(argc, argv, "", options, NULL)) != -1)
  {
    if (option != 't' && option != 's' && option != 'a')
    {
      return 1;
    }

    char *endptr;
    errno = 0;

    if (optarg[0] == '-')
    {
      printf("Arguments must be non-negative\n");
      return 1;
    }

    unsigned long value = strtoul(optarg, &endptr, 10);

    if (errno != 0 || endptr == optarg ||
        *endptr != '\0' || value > UINT32_MAX)
    {
      printf("Invalid argument\n");
      return 1;
    }

    switch (option)
    {
    case 't':
      threads_num = (uint32_t)value;
      break;
    case 's':
      seed = (uint32_t)value;
      seed_given = 1;
      break;
    case 'a':
      array_size = (uint32_t)value;
      break;
    }
  }

  if (optind != argc || !seed_given ||
      threads_num == 0 || array_size == 0 ||
      threads_num > array_size)
  {
    printf("Usage: %s --threads_num N --seed N --array_size N\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  pthread_t *threads = malloc(sizeof(pthread_t) * threads_num);
  struct SumArgs *args = calloc(threads_num, sizeof(struct SumArgs));

  if (array == NULL || threads == NULL || args == NULL)
  {
    printf("Memory allocation failed\n");
    free(array);
    free(threads);
    free(args);
    return 1;
  }

  // Generate array before measuring time
  GenerateArray(array, array_size, seed);

  struct timespec start_time;
  struct timespec finish_time;

  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Create threads
  for (uint32_t i = 0; i < threads_num; i++)
  {
    args[i].array = array;
    args[i].begin = (size_t)i * array_size / threads_num;
    args[i].end = (size_t)(i + 1) * array_size / threads_num;
    args[i].result = 0;

    int error = pthread_create(
        &threads[i], NULL, ThreadSum, &args[i]);

    if (error != 0)
    {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));

      for (uint32_t j = 0; j < i; j++)
      {
        pthread_join(threads[j], NULL);
      }

      free(args);
      free(threads);
      free(array);
      return 1;
    }
  }

  // Wait for threads and collect results
  int64_t total_sum = 0;

  for (uint32_t i = 0; i < threads_num; i++)
  {
    int error = pthread_join(threads[i], NULL);

    if (error != 0)
    {
      fprintf(stderr, "pthread_join: %s\n", strerror(error));
      return 1;
    }

    total_sum += args[i].result;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish_time);

  // Calculate elapsed time in milliseconds
  double elapsed_time =
      (finish_time.tv_sec - start_time.tv_sec) * 1000.0;

  elapsed_time +=
      (finish_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

  printf("Total: %" PRId64 "\n", total_sum);
  printf("Elapsed time: %.3f ms\n", elapsed_time);

  free(args);
  free(threads);
  free(array);

  return 0;
}
