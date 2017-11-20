#include "wator.h"
#include <pthread.h>


/*
 * Struttura della coda rappresentata attraverso una linked list
 * */
typedef struct queue
{
	void *info;
	struct queue *next;
}queue;

/*
 * Struttura della coda sincronizzata attraverso coda e mutex
 * */
typedef struct synqueue
{
	queue *f; /*Primo elemento*/
	queue *l; /*Ultimo elemento*/
	pthread_mutex_t mutex;
	pthread_cond_t cv;
}synqueue;

int enqueue(synqueue *q, void *m);
queue *dequeue(synqueue *q);
int isempty(synqueue *q);
void initqueue(synqueue *q);
