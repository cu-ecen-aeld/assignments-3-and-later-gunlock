#define _GNU_SOURCE
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/eventfd.h>
#include <sys/signal.h>
#include <sys/signalfd.h>
#include <sys/socket.h>

/*---------------- Constants -----------------*/
const char* OUTPUT_FILE_PATH = "/var/tmp/aesdsocketdata";
const int BACKLOG = 5;
const int AESD_PORT = 9000;
const int SEND_BUF_SIZE = 1024;

/*-------------- syslog helpers ---------------*/
#define LOG_TO_SYSLOG  // undefine to direct to stdout
#ifdef LOG_TO_SYSLOG
#define DEBUG_LOG(msg, ...) syslog(LOG_DEBUG, "Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) syslog(LOG_ERR, "Error | " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...) printf("Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("Error | " msg "\n", ##__VA_ARGS__)
#endif

int main(int argc, char** argv){
  
  int ret_val = EXIT_FAILURE;
  int sigfd = -1; // file descriptor accepting signals
  int sockfd = -1;

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
  if((sigfd = signalfd(-1 /* create fd*/, &mask, SFD_NONBLOCK)) != 0) goto cleanup;

  // vars for socket
  struct sockaddr_in sa = {
    .sin_family = AF_INET,
    .sin_port = htons(AESD_PORT),
    .sin_addr.s_addr = htonl(INADDR_ANY)
  };
  struct sockaddr_in client_sa = {0};
  socklen_t client_sa_len = sizeof(client_sa);
  char client_ip[INET_ADDRSTRLEN] = {0};
 
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



  ret_val = EXIT_SUCCESS;

  cleanup:
    if(ret_val == EXIT_FAILURE) {
      ERROR_LOG("%s", strerror(errno));
    }
  
    if(sigfd != -1) close(sigfd);
    if(sockfd != -1) close(sockfd);
    closelog(); 

    return ret_val; 
}
