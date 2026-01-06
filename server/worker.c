#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include "worker.h"
#include "utility.h"


void* thread_proc(void* argument){
  
  thread_arg_t* arg = (thread_arg_t*)argument;
  int clientfd = arg->sockfd;
  int shutdownfd = arg->shutdownfd;
  atomic_int* completed = arg->completed;
  FILE* outfile = arg->outfile;
  pthread_mutex_t* lock = arg->lock;
  free(arg);

  enum {POLLFD_SIZE = 2};
  struct pollfd pollfds[POLLFD_SIZE] = {
    [0] = { .fd = clientfd, .events = POLLIN},
    [1] = { .fd = shutdownfd, .events = POLLIN}
  };

  char* buffer = NULL;
  size_t buffer_size;
  char chunk[256];

  FILE* memstream = open_memstream(&buffer, &buffer_size);
  if(!memstream){
    DEBUG_LOG("Client [%d] failed to create memstream: %s", clientfd, strerror(errno));
    atomic_store(completed, 1);
    close(clientfd);
    return NULL;
  }

  bool err = false;
  bool con_closed = false;

  while(true){
    
    int poll_ret_val = poll(pollfds, POLLFD_SIZE, -1 /*infinite wait*/ );

    // #1 check error + EINTR: if true then continue polling
    if(poll_ret_val == -1 && errno == EINTR) continue;

    // #2 check for fatal error: If true then break from loop
    if(poll_ret_val == -1) {
      DEBUG_LOG("Client [%d]: Error on poll(): %s", clientfd, strerror(errno));
      err = true;
      break;
    }
   
    // #3 check for shutdown event: If true then break from loop
    if(pollfds[1].revents & POLLIN) {
      DEBUG_LOG("Client [%d]: Received shutdown event", clientfd);
      break;
    }

    // #4 check if data is ready to be read on socket: If true
    // then read sizeof(chunk) and add it memstream.  Break
    // from loop if recieved a message stop char '\n', on error
    // or if connection was closed by sender
    if(pollfds[0].revents & POLLIN) {
      DEBUG_LOG("Client [%d]: Socket has data ready to be read", clientfd);
      ssize_t n = recv(clientfd, chunk, sizeof(chunk), 0);
      if(n > 0 /* data read, copy to memstream*/) {
        fwrite(chunk, sizeof(char), n, memstream);
        // check for end of message (stop char is '\n') 
        if(chunk[n-1] == '\n') { // check for stop char '\n'
          break;
        }
      } else if (n == 0 /* connection closed by sender */) {
        con_closed = true;
        break;
      } else if (n == -1 /* recv() error */) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }
        err = true;
        break;
      }
    }
  }

  fclose(memstream); // finalizes buffer and buffer_size

  if(!err && buffer_size > 0){
    // we have some data.  Write it if we have a '\n' message
    if(buffer[buffer_size -1] != '\n') {
      ERROR_LOG("Client [%d]: Message missing newline terminated. Closing down client.", clientfd);
    } else { /* write to outfile */
      pthread_mutex_lock(lock);
      fwrite(buffer, sizeof(char), buffer_size, outfile); // COMMENT: add error handling
      fflush(outfile);
      pthread_mutex_unlock(lock);

      // read contents of outfile into buffer and then send over the socket
      int res_pread = -1;
      
      pthread_mutex_lock(lock);
      struct stat st;
      fstat(fileno(outfile), &st);
      
      char* file_buf = calloc(1, st.st_size);
      // pread() - atomic read at ofs 0
      if(file_buf) {
        res_pread = pread(fileno(outfile), file_buf, st.st_size, 0);
      }
      pthread_mutex_unlock(lock);

      if(file_buf && res_pread != -1 && !con_closed) {
        send(clientfd, file_buf, st.st_size, 0);
      }
      if(file_buf) free(file_buf);
    }
  }
  
  // free buffer created by memstream
  free(buffer);
  
  // set the completed flag so the main thread knows to 
  // join this thread id so it is not leaked
  atomic_store(completed, 1);
  close(clientfd);
  
  return NULL;
}


