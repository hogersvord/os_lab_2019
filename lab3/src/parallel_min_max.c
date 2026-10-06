#include <ctype.h>
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


int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    bool with_files = false;

    while (true) {
        int current_optind = optind ? optind : 1;

        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        int c = getopt_long(
            argc,
            argv,
            "f",
            options,
            &option_index
        );

        if (c == -1)
            break;

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0:
                        seed = atoi(optarg);
                        break;

                    case 1:
                        array_size = atoi(optarg);
                        break;

                    case 2:
                        pnum = atoi(optarg);
                        break;

                    case 3:
                        with_files = true;
                        break;

                    default:
                        printf(
                            "Index %d is out of options\n",
                            option_index
                        );
                }

                break;

            case 'f':
                with_files = true;
                break;

            case '?':
                break;

            default:
                printf(
                    "getopt returned character code 0%o?\n",
                    c
                );
        }
    }


    /*
        Проверяем аргументы.
    */

    if (optind < argc) {
        printf("Has at least one no option argument\n");
        return 1;
    }

    if (
        seed < 0 ||
        array_size <= 0 ||
        pnum <= 0 ||
        pnum > array_size
    ) {
        printf(
            "Usage: %s --seed \"num\" "
            "--array_size \"num\" --pnum \"num\"\n",
            argv[0]
        );

        return 1;
    }


    /*
        Создаём массив.
    */

    int *array = malloc(
        sizeof(int) * array_size
    );

    GenerateArray(
        array,
        array_size,
        seed
    );


    /*
        Для задания 3 создаём pipes.

        pipes[i][0] - чтение
        pipes[i][1] - запись
    */

    int (*pipes)[2] = NULL;

    if (!with_files) {
        pipes = malloc(
            sizeof(int[2]) * pnum
        );

        if (pipes == NULL) {
            printf("Cannot allocate pipes\n");
            free(array);
            return 1;
        }

        for (int i = 0; i < pnum; i++) {

            if (pipe(pipes[i]) == -1) {
                printf("Pipe failed!\n");

                free(pipes);
                free(array);

                return 1;
            }
        }
    }


    int active_child_processes = 0;

    struct timeval start_time;
    gettimeofday(&start_time, NULL);


    /*
        Создаём pnum дочерних процессов.
    */

    for (int i = 0; i < pnum; i++) {

        pid_t child_pid = fork();


        if (child_pid < 0) {
            printf("Fork failed!\n");

            free(array);

            if (pipes != NULL)
                free(pipes);

            return 1;
        }


        /*
            CHILD PROCESS
        */

        if (child_pid == 0) {

            /*
                Определяем часть массива,
                которую будет обрабатывать
                именно этот процесс.
            */

            int begin =
                i * array_size / pnum;

            int end =
                (i + 1) * array_size / pnum;


            /*
                Ищем локальные min и max.
            */

            struct MinMax local_min_max =
                GetMinMax(
                    array,
                    begin,
                    end
                );


            /*
                ЗАДАНИЕ 2

                Если указан --by_files,
                результат записываем в файл.
            */

            if (with_files) {

                char filename[64];

                snprintf(
                    filename,
                    sizeof(filename),
                    "result_%d.txt",
                    i
                );


                FILE *file =
                    fopen(filename, "w");


                if (file == NULL) {
                    printf(
                        "Cannot open file\n"
                    );

                    return 1;
                }


                fprintf(
                    file,
                    "%d %d\n",
                    local_min_max.min,
                    local_min_max.max
                );


                fclose(file);
            }


            /*
                ЗАДАНИЕ 3

                Если --by_files нет,
                передаём результат через pipe.
            */

            else {

                /*
                    Закрываем ненужные
                    дескрипторы pipe.
                */

                for (int j = 0; j < pnum; j++) {

                    close(pipes[j][0]);

                    if (j != i) {
                        close(
                            pipes[j][1]
                        );
                    }
                }


                /*
                    Отправляем структуру
                    родительскому процессу.
                */

                write(
                    pipes[i][1],
                    &local_min_max,
                    sizeof(local_min_max)
                );


                close(pipes[i][1]);
            }


            free(array);

            if (pipes != NULL)
                free(pipes);


            /*
                Завершаем ребёнка.
            */

            _exit(0);
        }


        /*
            Сюда попадает только родитель.
        */

        active_child_processes++;


        /*
            Родителю писать в pipe
            этого ребёнка уже не нужно.
        */

        if (!with_files) {
            close(pipes[i][1]);
        }
    }


    /*
        Родитель ждёт завершения
        всех дочерних процессов.
    */

    while (active_child_processes > 0) {

        wait(NULL);

        active_child_processes--;
    }


    /*
        Здесь будем хранить
        итоговые min и max.
    */

    struct MinMax min_max;

    min_max.min = INT_MAX;
    min_max.max = INT_MIN;


    /*
        Получаем результаты
        каждого процесса.
    */

    for (int i = 0; i < pnum; i++) {

        int min = INT_MAX;
        int max = INT_MIN;


        /*
            ЗАДАНИЕ 2

            Читаем из файла.
        */

        if (with_files) {

            char filename[64];

            snprintf(
                filename,
                sizeof(filename),
                "result_%d.txt",
                i
            );


            FILE *file =
                fopen(filename, "r");


            if (file == NULL) {
                printf(
                    "Cannot open file\n"
                );

                return 1;
            }


            fscanf(
                file,
                "%d %d",
                &min,
                &max
            );


            fclose(file);


            /*
                Файл больше не нужен.
            */

            remove(filename);
        }


        /*
            ЗАДАНИЕ 3

            Получаем структуру из pipe.
        */

        else {

            struct MinMax local_min_max;


            read(
                pipes[i][0],
                &local_min_max,
                sizeof(local_min_max)
            );


            close(pipes[i][0]);


            min =
                local_min_max.min;

            max =
                local_min_max.max;
        }


        /*
            Сравниваем результат ребёнка
            с общим результатом.
        */

        if (min < min_max.min) {
            min_max.min = min;
        }

        if (max > min_max.max) {
            min_max.max = max;
        }
    }


    struct timeval finish_time;

    gettimeofday(
        &finish_time,
        NULL
    );


    double elapsed_time =
        (
            finish_time.tv_sec -
            start_time.tv_sec
        ) * 1000.0;


    elapsed_time +=
        (
            finish_time.tv_usec -
            start_time.tv_usec
        ) / 1000.0;


    /*
        Освобождаем память.
    */

    if (pipes != NULL) {
        free(pipes);
    }

    free(array);


    /*
        Вывод результата.
    */

    printf(
        "Min: %d\n",
        min_max.min
    );

    printf(
        "Max: %d\n",
        min_max.max
    );

    printf(
        "Elapsed time: %fms\n",
        elapsed_time
    );

    fflush(NULL);

    return 0;
}