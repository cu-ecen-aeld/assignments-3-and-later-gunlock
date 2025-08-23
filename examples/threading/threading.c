#include "threading.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Optional: use these functions to add debug or error prints to your application
// #define DEBUG_LOG(msg, ...)
#define DEBUG_LOG(msg, ...) printf("threading: " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param) {
    // COMPLETED: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    // struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    unsigned long thread_id = pthread_self();

    DEBUG_LOG("Started threadfunc in thread %lu", thread_id);
    struct thread_data *data = (struct thread_data *)thread_param;

    DEBUG_LOG("Waiting %d ms before locking mutex in thread %lu", data->wait_to_obtain_ms,
              thread_id);
    usleep(data->wait_to_obtain_ms * 1000);

    DEBUG_LOG("Locking mutex in thread %lu", thread_id);
    pthread_mutex_lock(data->mutex);
    DEBUG_LOG("Waiting %d ms before releasing mutex in thread %lu", data->wait_to_release_ms,
              thread_id);
    usleep(data->wait_to_release_ms * 1000);
    data->thread_complete_success = true;
    pthread_mutex_unlock(data->mutex);
    DEBUG_LOG("Successfully unlocked mutex in thread %lu", thread_id);

    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms,
                                  int wait_to_release_ms) {
    /**
     * COMPLETED: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data
     * to created thread using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    DEBUG_LOG("start_thread_obtaining_mutex called. Current thread id is %lu", pthread_self());

    struct thread_data *data = malloc(sizeof(struct thread_data));
    if (!data) {
        ERROR_LOG("Failed to allocate memory for thread_data");
        return false;
    }

    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->mutex = mutex;
    data->thread_complete_success = false;

    return pthread_create(thread, NULL, threadfunc, (void *)data) == 0;
}
