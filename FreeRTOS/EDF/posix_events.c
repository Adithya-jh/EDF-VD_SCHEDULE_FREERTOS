/* posix_events.c
   A simple implementation of event functions using pthreads.
   These functions provide the POSIX port with the basic event primitives.
*/
#include <pthread.h>
#include <stdlib.h>

/* Define a structure to hold an event (a condition variable and its mutex). */
typedef struct EventStruct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} EventStruct;

/* Create a new event. Returns a pointer to the event structure. */
void* event_create(void)
{
    EventStruct* ev = (EventStruct*) malloc(sizeof(EventStruct));
    if (ev != NULL) {
        pthread_mutex_init(&ev->mutex, NULL);
        pthread_cond_init(&ev->cond, NULL);
    }
    return (void*)ev;
}

/* Delete an event and free associated resources. */
void event_delete(void* ev)
{
    if (ev != NULL) {
        EventStruct* e = (EventStruct*) ev;
        pthread_cond_destroy(&e->cond);
        pthread_mutex_destroy(&e->mutex);
        free(e);
    }
}

/* Wait on the event. This simple implementation waits indefinitely.
   A real implementation might also support a timeout.
*/
int event_wait(void* ev, unsigned int timeout_ms)
{
    (void)timeout_ms; /* unused parameter in this simple version */
    if (ev == NULL) return -1;
    EventStruct* e = (EventStruct*) ev;
    int ret = 0;
    pthread_mutex_lock(&e->mutex);
    ret = pthread_cond_wait(&e->cond, &e->mutex);
    pthread_mutex_unlock(&e->mutex);
    return ret;
}

/* Signal the event: wake up one waiting thread. */
int event_signal(void* ev)
{
    if (ev == NULL) return -1;
    EventStruct* e = (EventStruct*) ev;
    pthread_mutex_lock(&e->mutex);
    int ret = pthread_cond_signal(&e->cond);
    pthread_mutex_unlock(&e->mutex);
    return ret;
}
