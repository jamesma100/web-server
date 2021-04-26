#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char *shm_name = argv[1];
    int sleeptime_ms = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    printf("argument: %s\n", shm_name);
    printf("argument: %i\n", sleeptime_ms);
    printf("argument: %i\n", num_threads);
}
