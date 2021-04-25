#include "helper.h"
#include "request.h"
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

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

// CS537: Parse the new arguments too
void getargs(int *port, int *threads, int *buffers, int argc, char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }
  *port = atoi(argv[1]);
  *threads = atoi(argv[2]);
  *buffers = atoi(argv[3]);
}

void *worker(void *arg) {
  printf("Worker %lu created\n", pthread_self());
  while (1) {
    pthread_mutex_lock(&mu);
    // wait while no requests are in buffer
    while (is_empty == 1) {
      pthread_cond_wait(&buff_not_empty, &mu);
    }
    pthread_cond_signal(&buff_not_full);
    pthread_mutex_unlock(&mu);
  }
  return NULL;
}


int main(int argc, char *argv[])
{
  int listenfd, connfd, port, clientlen, threads, buffers;
  // char shm_name;
  struct sockaddr_in clientaddr;

  getargs(&port, &threads, &buffers, argc, argv);
  // int buffer[buffers];
  pthread_mutex_init(&mu, NULL);
  pthread_cond_init(&buff_not_empty, NULL);
  pthread_cond_init(&buff_not_full, NULL);
  is_full = 0;
  is_empty = 0;

  //
  // CS537 (Part B): Create & initialize the shared memory region...
  //

  // 
  // CS537 (Part A): Create some threads...
  //

    // setup threads
  pthread_t workers[threads];
  for (int i = 0; i < threads; i++) {
    pthread_create(&workers[i], NULL, worker, NULL);
  }

  listenfd = Open_listenfd(port);
  while (1) {
    // wait while buffer is full with requests
    pthread_mutex_lock(&mu);
    while (is_full == 1) {
      pthread_cond_wait(&buff_not_full, &mu);
    }

    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    // 
    // CS537 (Part A): In general, don't handle the request in the main thread.
    // Save the relevant info in a buffer and have one of the worker threads 
    // do the work. Also let the worker thread close the connection.
    // 
    requestHandle(connfd);
    Close(connfd);
    pthread_cond_signal(&buff_not_empty);
    pthread_mutex_unlock(&mu);
  }

  // kill mutext lock and cond variables
  pthread_mutex_destroy(&mu);
  pthread_cond_destroy(&buff_not_empty);
  pthread_cond_destroy(&buff_not_full);
}
