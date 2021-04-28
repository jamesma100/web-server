#include "helper.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "shm-slot.h"
#include <signal.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// P7 cond variables and mutex lock
pthread_mutex_t mu;
pthread_cond_t buff_not_empty;
pthread_cond_t buff_not_full;
volatile int is_full;
volatile int is_empty;
int *buffer;
int buffer_len;
slot_t *shm_ptr;
int threads;
int page_size;
char *shm_name;
int cur_worker_slot = 0;

void sigint_handler(int signal) {
  printf("Captured signal SIGINT %d\n", signal);
  // free shared memory
  page_size = getpagesize();
  munmap(shm_ptr, page_size);
  shm_unlink(shm_name);
  exit(0);
}

// CS537: Parse the new arguments too
void getargs(int *port, int *threads, int *buffers, char **shm_name, int argc, char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }
  *port = atoi(argv[1]);
  *threads = atoi(argv[2]);
  *buffers = atoi(argv[3]);
  *shm_name = argv[4];
}

// Traverse the input array from left to right and returns the index of the
// first null slot in the array
int get_empty() {
  // int buff_len = buffer_len_one == 0 ? (int) sizeof(buffer) / sizeof(buffer[0]) : 1; 
  for (int i = 0; i < buffer_len; i++) {
    if (buffer[i] == -1) {
      return i;
    }
  }

  // should never hit here? 
  printf("get_empty() was called when buffer array was full\n");
  return -1;
}

// Traverse the input array from right to left and returns the index of the
// first null slot in the array
int get_full() {
  // int buff_len = buffer_len_one == 0 ? (int) sizeof(buffer) / sizeof(buffer[0]) : 1; 
  // printf("size of buffer is %d\n", (int) sizeof buffer);
  // printf("size of buffer[0] is %d\n", (int) sizeof buffer[0]);
  // printf("buff_len is %d\n", buff_len);
  for (int i = buffer_len - 1; i >= 0; i--) {
    if (buffer[i] != -1) {
      return i;
    }
  }

  // should never hit here? 
  printf("get_full() was called when buffer array was empty\n");
  return -1;
}

void *worker(void *arg) {
  
  printf("Worker %lu created ", pthread_self());
  for (int i = 0; i < threads; ++i) {
    if (shm_ptr[i].tid == 0) {
      shm_ptr[i].tid = pthread_self();
      printf("on slot %d\n", i);
      break;
    }
  }

  cur_worker_slot++;
  int is_static;
  while (1) {
    pthread_mutex_lock(&mu);
    // wait while no requests are in buffer
    while (is_empty == 1) {
      pthread_cond_wait(&buff_not_empty, &mu);
    }

    // grab a connection
    int buff_i = get_full();
    // printf("buff_i is %d\n", buff_i);
    int connfd = buffer[buff_i];
    // printf("connfd is %d\n", connfd);
    buffer[buff_i] = -1;
    is_full = 0;
    if (buffer[0] == -1) {
      is_empty = 1;
    }
    pthread_mutex_unlock(&mu);

    pthread_mutex_lock(&mu);
    // handle connection async
    printf("worker %lu accepted connection %d\n", pthread_self(), connfd);
    is_static = requestHandle(connfd);
    Close(connfd);
    printf("handled connection %d\n", connfd);
    // handle statistics
    for (int i = 0; i < threads; ++i) {
      if (pthread_self() == shm_ptr[i].tid) {
        if (is_static == 1) {
          shm_ptr[i].static_req++;
        } else if (is_static == 0) {
          shm_ptr[i].dynamic_req++;
        }
        shm_ptr[i].total_req++;
        printf("stats for worker %d\n", i);
        printf("total static: %d\n", shm_ptr[i].static_req);
        printf("total dynamic: %d\n", shm_ptr[i].dynamic_req);
        printf("total: %d\n", shm_ptr[i].total_req);
        printf("--------total stats----------\n");
        long unsigned tid;
        int static_req, dynamic_req, total_req;
        for (int i = 0; i < threads; ++i) {
            tid = shm_ptr[i].tid;
            static_req = shm_ptr[i].static_req;
            dynamic_req = shm_ptr[i].dynamic_req;
            total_req = shm_ptr[i].total_req;
            printf("%lu : %d %d %d\n", tid, total_req, static_req, dynamic_req);
        }
        printf("-----------------------------\n");

        break;
      }
    }
    
    pthread_mutex_unlock(&mu);

    // update buffers
    pthread_mutex_lock(&mu);
    // buffer[buff_i] = -1;
    // is_full = 0;
    // if (buffer[0] == -1) {
    //   is_empty = 1;
    // }
    pthread_cond_signal(&buff_not_full);
    pthread_mutex_unlock(&mu);

    
  }
  return NULL;
}


int main(int argc, char *argv[])
{
  int listenfd, connfd, port, clientlen, buff_len;
  
  struct sockaddr_in clientaddr;

  getargs(&port, &threads, &buff_len, &shm_name, argc, argv);
  printf("args: %d, %d, %d, %s\n", port, threads, buff_len, shm_name);
  // check invalid arguments
  if (port <= 2000 || buff_len <= 0 || threads <= 0) {
    return 1;
  }
  buffer_len = buff_len;
 
  // initalize buffer
  buffer = malloc(buff_len * sizeof(int));
  // printf("main len is %d\n", buff_len);
  for (int i = 0; i < buff_len; i++) {
    buffer[i] = -1;
  }

  // initialize necessary variables + locks + cond
  is_full = 0;
  is_empty = 1;
  pthread_mutex_init(&mu, NULL);
  pthread_cond_init(&buff_not_empty, NULL);
  pthread_cond_init(&buff_not_full, NULL);
  
  // CS537 (Part B): Create & initialize the shared memory region...
  //
  // initialize shared memory
  page_size = getpagesize();
  int shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, 0660);
  if (shm_fd < 0) {
    printf("shm_open exit with ret code -1\n");
    return 1;
  }
  
  ftruncate(shm_fd, page_size);
  shm_ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  
  if (shm_ptr == MAP_FAILED) {
    return 1;
  }
  printf("shared mem created\n");

  // register signal handler
  signal(SIGINT, sigint_handler);

  // 
  // CS537 (Part A): Create some threads...
  //

    // setup threads
  pthread_t workers[threads];
  for (int i = 0; i < threads; i++) {
    pthread_create(&workers[i], NULL, worker, NULL);
    // shm_ptr[i].tid = pthread_self();
    // shm_ptr[i].static_req = 0;
    // shm_ptr[i].dyanmic_req = 0;
    // shm_ptr[i].total_req = 0;
  }

  listenfd = Open_listenfd(port);
  while (1) {
    // wait while buffer is full with requests
    pthread_mutex_lock(&mu);
    while (is_full == 1) {
      pthread_cond_wait(&buff_not_full, &mu);
    }
    // printf("checkpoint 0\n");

    clientlen = sizeof(clientaddr);
    // printf("checkpoint 1\n");

    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    // printf("conn fd is %d\n", connfd);
    // printf("accepted connection %d\n", connfd);

    // requestHandle(connfd);
    // Close(connfd);
    // printf("ended connection %d\n", connfd);
    
    // 
    // CS537 (Part A): In general, don't handle the request in the main thread.
    // Save the relevant info in a buffer and have one of the worker threads 
    // do the work. Also let the worker thread close the connection.

    // Find an empty slot in the buffer array and put the connection there
    int buff_i = get_empty();
    buffer[buff_i] = connfd;
    // printf("buffer 0 is %d\n", buffer[0]);
    is_empty = 0;
    if (buffer[buff_len] != -1) {
      is_full = 1;
    }

    pthread_cond_signal(&buff_not_empty);
    pthread_mutex_unlock(&mu);
  }

  // kill mutext lock and cond variables
  pthread_mutex_destroy(&mu);
  pthread_cond_destroy(&buff_not_empty);
  pthread_cond_destroy(&buff_not_full);

  // free malloc'd bytes
  free(buffer);
  return 0;
}

