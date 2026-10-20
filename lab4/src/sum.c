#include "sum.h"

int64_t Sum(const int *array, size_t begin, size_t end) {
    int64_t sum = 0;

    for (size_t i = begin; i < end; ++i) {
        sum += array[i];
    }

    return sum;
}
