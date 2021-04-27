#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        return 1;
    }
    char *shm_name = argv[1];
    int sleeptime_ms = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    printf("args: %s, %i, %i\n", shm_name, sleeptime_ms, num_threads);    
    int shm_fd = shm_open(shm_name, O_RDWR, 0660);
    if (shm_fd == -1) {
        return 1;
    }
    
    return 0;
}
