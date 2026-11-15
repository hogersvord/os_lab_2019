#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// Данные, которые получает каждый поток
typedef struct {
    int id;
    long long start;
    long long count;
    uint64_t mod;
    uint64_t *result;
    pthread_mutex_t *mutex;
} ThreadData;

// Функция потока
void *calculate_part(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    uint64_t part = 1 % data->mod;

    // Вычисление своей части факториала
    for (long long j = 0; j < data->count; ++j) {
        uint64_t x = (uint64_t)(data->start + j);
        part = (uint64_t)((__uint128_t)part * x % data->mod);
    }

    // Обновление общего результата
    pthread_mutex_lock(data->mutex);

    *(data->result) =
        (uint64_t)((__uint128_t)(*(data->result)) * part % data->mod);

    printf("Поток %d: начало=%lld, количество=%lld, часть=%" PRIu64 "\n",
           data->id, data->start, data->count, part);

    pthread_mutex_unlock(data->mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    long long k = -1, pnum = 0, mod = 0;

    // Параметры командной строки
    static struct option options[] = {
        {"k", required_argument, NULL, 'k'},
        {"pnum", required_argument, NULL, 'p'},
        {"mod", required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "k:p:m:",
                              options, NULL)) != -1) {
        if (opt == '?')
            return 1;

        errno = 0;
        char *end;
        long long number = strtoll(optarg, &end, 10);

        if (errno != 0 || end == optarg || *end != '\0') {
            fprintf(stderr, "Неверное число: %s\n", optarg);
            return 1;
        }

        if (opt == 'k') k = number;
        if (opt == 'p') pnum = number;
        if (opt == 'm') mod = number;
    }

    // Проверка параметров
    if (k < 0 || pnum <= 0 || pnum > INT_MAX ||
        mod <= 0 || optind != argc) {
        fprintf(stderr,
                "Использование: %s -k 10 --pnum=4 --mod=10\n",
                argv[0]);
        return 1;
    }

    // Выделение памяти для потоков
    pthread_t *threads = calloc((size_t)pnum, sizeof(*threads));
    ThreadData *data = calloc((size_t)pnum, sizeof(*data));

    if (!threads || !data) {
        perror("calloc");
        free(threads);
        free(data);
        return 1;
    }

    // Общий результат и мьютекс
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    uint64_t result = 1 % (uint64_t)mod;

    // Равномерное распределение множителей
    long long base = k / pnum;
    long long remainder = k % pnum;
    long long next = 1;
    int created = 0;

    // Создание потоков
    for (int i = 0; i < pnum; ++i) {
        long long count = base + (i < remainder);

        data[i] = (ThreadData){
            i + 1, next, count, (uint64_t)mod,
            &result, &mutex
        };

        int err = pthread_create(
            &threads[i], NULL, calculate_part, &data[i]
        );

        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            break;
        }

        ++created;
        if (i < pnum - 1)
            next += count;
    }

    // Ожидание завершения потоков
    for (int i = 0; i < created; ++i)
        pthread_join(threads[i], NULL);

    // Вывод результата
    if (created == pnum)
        printf("%lld! mod %lld = %" PRIu64 "\n",
               k, mod, result);

    // Освобождение ресурсов
    pthread_mutex_destroy(&mutex);
    free(threads);
    free(data);

    return created == pnum ? 0 : 1;
}
