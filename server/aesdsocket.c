#define _GNU_SOURCE // for accept4()
#include <arpa/inet.h>
#include <errno.h>  // IWYU pragma: keep
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h> // IWYU pragma: keep
#include <string.h>
#include <syslog.h>
#include <sys/eventfd.h>
#include <sys/signal.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include "list.h"

/*---------------- Constants ------------------*/
const char* OUTPUT_FILE_PATH = "/var/tmp/aesdsocketdata";
const int BACKLOG = 5;
const int AESD_PORT = 9000;
const int SEND_BUF_SIZE = 1024;

/*---------------- Thread Args  ---------------*/
struct thread_arg_t {
  int sockfd;
  int shutdownfd;
  atomic_int* completed;
} thread_arg_t;

/*-------------- syslog Helpers ---------------*/
// #define LOG_TO_SYSLOG  // undefine to direct to stdout
#ifdef LOG_TO_SYSLOG
#define DEBUG_LOG(msg, ...) syslog(LOG_DEBUG, "Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) syslog(LOG_ERR, "Error | " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...) printf("Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("Error | " msg "\n", ##__VA_ARGS__)
#endif

/*---------------- thread proc  ---------------*/
void* thread_proc(void* arg){
  (void)arg;
  return NULL;
}


int main(int argc, char** argv){
  
  int ret_val = EXIT_FAILURE;
  int sigfd = -1;      // file descriptor accepting signals
  int sockfd = -1;     // socket
  int shutdownfd = -1; // to signal worker threads to shutdown
  
  // parse args
  bool daemon = false;
  if(argc == 2 && (strcmp(argv[1], "-d") == 0)){
    daemon = true;
  }
 
  // open syslog
  openlog(NULL, 0, LOG_USER);

  // setup signal file descriptor for accepting SIGINT and SIGTERM signals
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  // block signals SIGINT and SIGTERM to all
  if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) goto cleanup;
  if((sigfd = signalfd(-1 /* create fd*/, &mask, SFD_NONBLOCK)) == -1) goto cleanup;

  // event file descriptor to broadcast to workers to shutdown
  if((shutdownfd = eventfd(0, EFD_NONBLOCK)) == -1) goto cleanup;

  // vars for socket
  struct sockaddr_in sa = {
    .sin_family = AF_INET,
    .sin_port = htons(AESD_PORT),
    .sin_addr.s_addr = htonl(INADDR_ANY)
  };
  struct sockaddr_in client_sa = {0};
  socklen_t client_sa_len = sizeof(client_sa);

 
  // open listening socket
  if ((sockfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) == -1) goto cleanup; 
  DEBUG_LOG("Socket open was successful");
  // Configure socket so can rebind immediatedly, in case of restart or
  // crash, on same port...and not get hung up by a port's TIME_WAIT state
  int opt_on = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_on, sizeof(opt_on));

  // bind socket
  if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1) goto cleanup; 
  DEBUG_LOG("Socket bind was successful");

  // daemonize after bind
  if(daemon) {
    pid_t pid = fork();
    if(pid < 0)   /* fork failed*/ goto cleanup;
    if(pid > 0) { /* fork success, in parent */ 
      ret_val = EXIT_SUCCESS;
      goto cleanup;
    }
    // in child
    setsid();
  }
  
  // listen
  if (listen(sockfd, BACKLOG) == -1) goto cleanup;
  DEBUG_LOG("Server started on port %d", AESD_PORT);

  // setup pollfd array for file descriptors to poll
  enum { POLLFD_SIZE = 2};
  struct pollfd pollfds [POLLFD_SIZE] = {
    [0] = { .fd = sigfd,  .events = POLLIN},
    [1] = { .fd = sockfd, .events = POLLIN}
  };

  // create list to hold thread ids
  node_t* tid_list = NULL;

  // event loop
  while(true){

    // start polling
    int poll_ret_val = poll(pollfds, POLLFD_SIZE, -1 /* infinite timeout */);
    
    // check for poll() error 
    if(poll_ret_val == -1) { 
      if(errno == EINTR){
        continue;
      } 
      break;
    }

    // check for signal
    if(pollfds[0].revents & POLLIN) {
      struct signalfd_siginfo siginfo;
      read(sigfd, &siginfo, sizeof(siginfo));
      DEBUG_LOG("Received shutdown signal");
      break;
    }

    // new connection
    if(pollfds[1].revents & POLLIN){
      int clientfd = accept4(sockfd, 
                             (struct sockaddr *)&client_sa, 
                             &client_sa_len,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
      if(clientfd == -1) {
        ERROR_LOG("accept4() return error %s", strerror(errno));
        continue;
      }

      // get client IP
      char ipaddr[INET_ADDRSTRLEN] = {0};
      inet_ntop(AF_INET, &client_sa.sin_addr, ipaddr, sizeof(ipaddr));
      syslog(LOG_DEBUG, "Accepted connection from %s", ipaddr);
      DEBUG_LOG("Accepted connection...");

      // spawn worker
      DEBUG_LOG("Spawning worker thread...");
      struct thread_arg_t* arg = calloc(1, sizeof(thread_arg_t));
      node_t* tid_item = calloc(1, sizeof(node_t));
      arg->shutdownfd = shutdownfd;
      arg->sockfd = clientfd;
      arg->completed = &tid_item->completed;
      pthread_create(&tid_item->tid, NULL, thread_proc, arg);
      
      // cleanup any threads that are completed
      if(tid_list) free_finished_threads(&tid_list);
      
      // add this thread to thread id list
      if(tid_list) {
        push_front(&tid_list, tid_item);
      } else {
        tid_list = tid_item;
      }
    }  // end of new connection block
  }

  // broadcast to all workers to shutdown
  uint32_t val = 1;
  write(shutdownfd, &val, sizeof(val));
  DEBUG_LOG("Broadcasting shutdown to worker threads from main thread");

  // All threads have been signaled to shutdown...safe to join them 
  // to cleanup properly
  if(tid_list) {
    node_t* cur = tid_list;
    node_t* next = NULL;
    while(cur != NULL) {
      next = cur->next;
      pthread_join(cur->tid, NULL);
      cur = next;
    }
  }

  // free thread id list
  free_list(&tid_list);

  // success if here, set return code
  DEBUG_LOG("Shutting down server");
  ret_val = EXIT_SUCCESS;

  cleanup:
    if(ret_val == EXIT_FAILURE) {
      ERROR_LOG("%s", strerror(errno));
    }
  
    if(sigfd != -1) close(sigfd);
    if(sockfd != -1) close(sockfd);
    if(shutdownfd != -1) close(shutdownfd);
    closelog(); 

    return ret_val; 
}
