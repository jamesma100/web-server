#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "shm-slot.h"
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("error: incorrect number of arguments\n");
        return 1;
    }
    char *shm_name = argv[1];
    int sleeptime_ms = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
 
    if (sleeptime_ms < 0) {
        printf("error: sleeptime_ms\n");
        return 1;
    }
    if (num_threads < 0){
        printf("error: num_threads\n");
        return 1;
    }

    int shm_fd = shm_open(shm_name, O_RDWR, 0660);
    if (shm_fd == -1) {
        printf("error: shared memory segment does not exist\n");
        return 1;
    }
    int page_size = getpagesize();
    slot_t *shm_ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = sleeptime_ms * 1000000;
    
    int it = 1;
    long unsigned tid;
    int static_req, dynamic_req, total_req;
    while (1) {
        nanosleep(&ts, NULL); 
        printf("\n%d\n", it);
        it++;
        for (int i = 0; i < num_threads; ++i) {
            tid = shm_ptr[i].tid;
            static_req = shm_ptr[i].static_req;
            dynamic_req = shm_ptr[i].dynamic_req;
            total_req = shm_ptr[i].total_req;
            printf("%lu : %d %d %d\n", tid, total_req, static_req, dynamic_req);
        }
    }
    return 0;
}
