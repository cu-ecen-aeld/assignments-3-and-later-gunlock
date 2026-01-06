#pragma once
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

typedef struct thread_arg_t {
  int sockfd;
  int shutdownfd;
  atomic_int* completed;
  FILE* outfile;
  pthread_mutex_t* lock;
} thread_arg_t;

void* thread_proc(void* arg);
