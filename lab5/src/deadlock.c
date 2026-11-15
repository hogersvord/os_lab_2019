#include <stdio.h>
#include <pthread.h>

// Два мьютекса
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

// Барьер для синхронизации потоков
pthread_barrier_t barrier;

// Первый поток
void *thread1_func(void *arg) {
    pthread_mutex_lock(&mutex1);
    printf("Поток 1: захватил mutex1\n");

    pthread_barrier_wait(&barrier);

    printf("Поток 1: ожидает mutex2\n");
    pthread_mutex_lock(&mutex2);

    printf("Поток 1: захватил mutex2\n");

    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);

    return NULL;
}

// Второй поток
void *thread2_func(void *arg) {
    pthread_mutex_lock(&mutex2);
    printf("Поток 2: захватил mutex2\n");

    pthread_barrier_wait(&barrier);

    printf("Поток 2: ожидает mutex1\n");
    pthread_mutex_lock(&mutex1);

    printf("Поток 2: захватил mutex1\n");

    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);

    return NULL;
}

int main() {
    pthread_t thread1, thread2;

    setbuf(stdout, NULL);

    pthread_barrier_init(&barrier, NULL, 2);

    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);

    printf("Программа завершена\n");

    return 0;
}
