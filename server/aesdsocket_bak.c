#include <arpa/inet.h>
#include <errno.h> // IWYU pragma: keep
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#define _GNU_SOURCE  // for accept4() 
#include <sys/socket.h>

// Uncomment to log to syslog, otherwise, stdout
#define LOG_TO_SYSLOG

#ifdef LOG_TO_SYSLOG
#define DEBUG_LOG(msg, ...) syslog(LOG_DEBUG, "Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) syslog(LOG_ERR, "Error | " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...) printf("Debug | " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("Error | " msg "\n", ##__VA_ARGS__)
#endif

// Constants and globals
const char *OUTPUT_FILE_PATH = "/var/tmp/aesdsocketdata";
const int BACKLOG = 5;
const int AESD_PORT = 9000;
const int SEND_BUF_SIZE = 1024;



bool g_kill_signal = false;
int g_signal_received;

/*
 * Signal Handler
 *
 * Signals Handled:  SIGINT  SIGTERM
 *
 * Sets:  g_kill_signal to true
 *        g_signal_received to the signal recieved (SIGINT or SIGTERM)
 */
static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        g_signal_received = signum;
        g_kill_signal = true;
    }
}

/*
 * Reads from a socket until a new line is encounterd ('\n')
 * and returns the line read.
 *
 * Parameters:
 *   sockfd [in]  - opened socket file descriptor
 *   line   [out] - received data. Includes '\n' and is null terminated
 *
 * Returns:  0 on success
 *          -1 on failure
 *
 * Memory Management:
 *   Caller frees line on success
 *
 * Assumptions:
 *   (1) Stream will not contain null characters
 *   (2) The packet is '\n' terminated
 */
ssize_t readline(int sockfd, char **line) {
    const int ALLOC_STEP = 128;
    size_t capacity = ALLOC_STEP;
    ssize_t bytes_read = 0;
    size_t pos = 0;  // write position
    bool end_of_packet_found = false;

    if ((*line = malloc(ALLOC_STEP)) == NULL) {
        return -1;
    }

    while (true) {
        if (pos >= capacity - 1) {  // make room for null termination
            // reallocation needed
            capacity += ALLOC_STEP;
            char *tmp = realloc(*line, capacity);
            if (NULL == tmp) {
                goto exit;
            }
            *line = tmp;
        }

        if ((bytes_read = read(sockfd, *line + pos, capacity - pos - 1)) <= 0) {
            if (bytes_read < 0) {  // error
                ERROR_LOG("Error reading from client socket: %s", strerror(errno));
                goto exit;
            } else {  // connection closed, bytes_read == 0
                ERROR_LOG("Client closed connection: %s", strerror(errno));
                goto exit;
            }
        }

        // see if the last byte read is '\n' indicating end of packet
        if (bytes_read > 0 && (*line)[pos + bytes_read - 1] == '\n') {
            pos += bytes_read;
            break;  // at end of packet
        } else {
            pos += bytes_read;
        }
    }

    (*line)[pos] = '\0';
    end_of_packet_found = true;

exit:
    if (!end_of_packet_found && *line) {
        free(*line);
        *line = NULL;
    }
    return end_of_packet_found ? 0 : -1;
}

void* thread_proc(void* arg){
   (void)arg; // silence clangd
    //
    char* buf = NULL;
    size_t size = 0;
    FILE* recv_stream = open_memstream(&buf, &size); // stream to read from socket


    // close stream and free buffer
    fclose(recv_stream);
    if(buf){
        free(buf);
    }

    return NULL;
}

// Setups and returns the signal file descriptor for SIGINT and SIGTERM
// Parameters:
//   mask [out] - configures mask for SIGINT and SIGTERM
// Returns
//   signal file descriptor setup for non-blocking
int init_signals(sigset_t* mask) {
    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    sigaddset(mask, SIGTERM);
    // when first argument is -1, then the call creates a new file descriptor
    return signalfd(-1, mask, SFD_NONBLOCK);
}

void start_server(bool daemonize) {

    struct sockaddr_in sa = {0};
    struct sockaddr_in client_sa = {0};
    socklen_t client_sa_len = sizeof(client_sa);
    int sockfd = -1;
    char client_ip[INET_ADDRSTRLEN] = {0};
   
    sigset_t mask;
    int sigfd = init_signals(&mask);
    // TODO: handle errors from init_signals() (i.e. if sigfd < 0)
    
    /**
     * Setup listening socket
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        ERROR_LOG("%s", strerror(errno));
        close(sockfd);
        return;
    }
    DEBUG_LOG("Socket open was successful");

    // Configure socket so can rebind immediatedly, in case of restart or
    // crash, on same port...and not get hung up by a port's TIME_WAIT state
    int opt_on = 1;
    setsockopt(sockfd,
               SOL_SOCKET,    // Level Option - manipulate options at the sockets API level
               SO_REUSEADDR,  // Option
               &opt_on,       // Option is set to on if is set to 1, off if set to 0
               sizeof(opt_on));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(AESD_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        ERROR_LOG("%s", strerror(errno));
        close(sockfd);
        return;
    }
    DEBUG_LOG("Socket bind was successful");

    // daemonize after bind
    if (daemonize) {
        pid_t pid = fork();
        if (pid < 0) {
            ERROR_LOG("Fork failed: %s", strerror(errno));
            close(sockfd);
            return;
        }
        if (pid > 0) {  // parent
            if (sockfd >= 0) {
                close(sockfd);
            }
            exit(EXIT_SUCCESS);  // parent exits
        }

        // child
        setsid();
    }

    if (listen(sockfd, BACKLOG) == -1) {
        ERROR_LOG("%s", strerror(errno));
        close(sockfd);
        return;
    }
    DEBUG_LOG("Socket listen was successful");




 
}

int main(int argc, char **argv) {
    bool daemon = (argc == 2) && (strcmp(argv[1], "-d") == 0);

    openlog(NULL, 0, LOG_USER);

    struct sigaction sigact = {0};
    struct sockaddr_in sa = {0};
    struct sockaddr_in client_sa = {0};
    socklen_t client_sa_len = sizeof(client_sa);
    int sockfd = -1;
    FILE *outputFile = NULL;
    char client_ip[INET_ADDRSTRLEN] = {0};
    const char *OUTPUT_FILE_PATH = "/var/tmp/aesdsocketdata";
    const int BACKLOG = 5;
    const int AESD_PORT = 9000;
    const int SEND_BUF_SIZE = 1024;

    /**
     * Setup signals
     */
    int signals[] = {SIGINT, SIGTERM};
    sigact.sa_handler = signal_handler;
    for (size_t i = 0; i < sizeof(signals) / sizeof(int); i++) {
        if (sigaction(signals[i], &sigact, NULL) == -1) {
            ERROR_LOG("%s", strerror(errno));
            goto exit;
        }
    }

    /**
     * Setup listening socket
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        ERROR_LOG("%s", strerror(errno));
        goto exit;
    }
    DEBUG_LOG("Socket open was successful");

    // Configure socket so can rebind immediatedly, in case of restart or
    // crash, on same port...and not get hung up by a port's TIME_WAIT state
    int opt_on = 1;
    setsockopt(sockfd,
               SOL_SOCKET,    // Level Option - manipulate options at the sockets API level
               SO_REUSEADDR,  // Option
               &opt_on,       // Option is set to on if is set to 1, off if set to 0
               sizeof(opt_on));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(AESD_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        ERROR_LOG("%s", strerror(errno));
        goto exit;
    }
    DEBUG_LOG("Socket bind was successful");

    // daemonize after bind
    if (daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            ERROR_LOG("Fork failed: %s", strerror(errno));
            goto exit;
        }
        if (pid > 0) {  // parent
            if (sockfd >= 0) {
                close(sockfd);
            }
            exit(EXIT_SUCCESS);  // parent exits
        }

        // child
        setsid();
    }

    if (listen(sockfd, BACKLOG) == -1) {
        ERROR_LOG("%s", strerror(errno));
        goto exit;
    }
    DEBUG_LOG("Socket listen was successful");

    /**
     * Open output file
     */
    if ((outputFile = fopen(OUTPUT_FILE_PATH, "w+")) == NULL) {
        ERROR_LOG("%s", strerror(errno));
        goto exit;
    }

    /**
     * Accept loop
     */
    do {
        DEBUG_LOG("Waiting for clients to connect...");

        memset(&client_sa, 0, sizeof(client_sa));
        int clientfd = accept(sockfd, (struct sockaddr *)&client_sa, &client_sa_len);
        if (clientfd == -1) {
            ERROR_LOG("%s", strerror(errno));
            goto exit;
        }
        DEBUG_LOG("Client connected.");

        // get client ip address
        memset(client_ip, 0, sizeof(client_ip));
        inet_ntop(AF_INET, &client_sa.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        // read from clientfd
        char *buf;
        if (readline(clientfd, &buf) == -1 || NULL == buf) {
            ERROR_LOG("%s", strerror(errno));
            close(clientfd);
            goto exit;
        }

        // Append received data to output file
        fseek(outputFile, 0, SEEK_END);
        size_t len = strlen(buf);
        if (fwrite(buf, sizeof(char), len, outputFile) != len) {
            ERROR_LOG("Failed to write to output file: %s", strerror(errno));
            free(buf);
            close(clientfd);
            goto exit;
        }
        if (fflush(outputFile) != 0) {
            ERROR_LOG("Failed to flush to output file: %s", strerror(errno));
            free(buf);
            close(clientfd);
            goto exit;
        }
        free(buf);
        buf = NULL;

        // send back complete contents of the file to the client
        fseek(outputFile, 0, SEEK_SET);
        char send_buf[SEND_BUF_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(send_buf, sizeof(char), sizeof(send_buf), outputFile)) > 0) {
            size_t bytes_written = 0;
            while (bytes_written < bytes_read) {
                ssize_t result =
                    write(clientfd, send_buf + bytes_written, bytes_read - bytes_written);
                if (result == -1) {
                    ERROR_LOG("Data send to client failed: %s", strerror(errno));
                    close(clientfd);
                    goto exit;
                }
                bytes_written += result;
            }
        }

        if (ferror(outputFile)) {
            ERROR_LOG("Error reading from output file: %s", strerror(errno));
            close(clientfd);
            goto exit;
        }

        close(clientfd);

    } while (!g_kill_signal);

exit:
    if (g_signal_received) {
        const char *sig = strsignal(g_signal_received);
        DEBUG_LOG("Shutting down. Received %s.", sig != NULL ? sig : "unknown signal number");
        // Logging this per assignment
        syslog(LOG_DEBUG, "Caught signal, exiting");
    }
    if (sockfd >= 0)
        close(sockfd);
    if (outputFile != NULL)
        fclose(outputFile);
#ifdef LOG_SYSLOG
    closelog();
#endif
    if (remove(OUTPUT_FILE_PATH) == -1) {
        ERROR_LOG("Error on deleting %s: %s", OUTPUT_FILE_PATH, strerror(errno));
    }
    return g_signal_received ? 0 : -1;
}
