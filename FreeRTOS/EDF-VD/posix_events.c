/* posix_events.c
   A minimal implementation of event_create, event_delete, event_wait, event_signal
   using pthreads for the FreeRTOS POSIX port.
*/

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

/* Our internal event structure: a condition variable + mutex. */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} PosixEvent_t;

void* event_create(void)
{
    PosixEvent_t *evt = (PosixEvent_t*)malloc(sizeof(PosixEvent_t));
    if(evt == NULL) {
        return NULL;
    }
    pthread_mutex_init(&evt->mutex, NULL);
    pthread_cond_init(&evt->cond, NULL);
    return evt;
}

void event_delete(void* ev)
{
    if(ev == NULL) {
        return;
    }
    PosixEvent_t *evt = (PosixEvent_t*) ev;
    pthread_cond_destroy(&evt->cond);
    pthread_mutex_destroy(&evt->mutex);
    free(evt);
}

int event_wait(void* ev, unsigned int timeout_ms)
{
    /* Mark timeout_ms as unused. */
    (void)timeout_ms;

    if(ev == NULL){
        return -1;
    }
    PosixEvent_t *evt = (PosixEvent_t*) ev;

    pthread_mutex_lock(&evt->mutex);
    /* Minimal blocking wait. For real timeouts, you'd do pthread_cond_timedwait. */
    int ret = pthread_cond_wait(&evt->cond, &evt->mutex);
    pthread_mutex_unlock(&evt->mutex);

    return ret;
}


int event_signal(void* ev)
{
    if(ev == NULL) {
        return -1;
    }
    PosixEvent_t *evt = (PosixEvent_t*) ev;
    pthread_mutex_lock(&evt->mutex);

    int ret = pthread_cond_signal(&evt->cond);

    pthread_mutex_unlock(&evt->mutex);
    return ret;
}
