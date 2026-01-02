#pragma once
#include <pthread.h>
#include <stdatomic.h>

typedef struct node_t {
  pthread_t tid;
  atomic_int completed;
  struct node_t* next;
} node_t;

node_t* init_node();
void push_front(node_t** head, node_t* item);
void free_finished_threads(node_t** head);
