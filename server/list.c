#include "list.h"
#include <stdlib.h>

node_t* init_node(){
  return calloc(1, sizeof(node_t));
}

void push_front(node_t** head, node_t* item) {
  if(NULL == head || NULL == item) {
    return;
  }
  
  item->next = *head;
  *head= item;
}

void free_finished_threads(node_t** head) {

  node_t** indirect = head;

  while(*indirect != NULL){
    node_t* current = *indirect;
    
    // check if thread is completed
    if(atomic_load(&current->completed)){
      *indirect = current->next;
#ifndef _DEBUG_NO_THREADS
      pthread_join(current->tid, NULL);
#endif
      free(current);
    } else {
      indirect = &current->next; 
    }
  }
}
