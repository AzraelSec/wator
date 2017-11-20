#include "synqueue.h"
 void initqueue(synqueue *q)
 {
	 q->f = q->l = NULL;
	 pthread_mutex_init(&(q->mutex), NULL);
	 pthread_cond_init(&(q->cv), NULL);
 }
 
/*La funzione aggiunge in coda*/
int enqueue(synqueue *q, void *m)
{
	queue *tmp;
	
	if(!q) return 0;
	if(!(tmp = (queue *)malloc(sizeof(queue)))) return 0;
	tmp->info = m;
	tmp->next = NULL;
	pthread_mutex_lock(&(q->mutex));
	
	if(!(q->f))
		q->f = q->l = tmp;	
	else
	{
		q->l->next = tmp;
		q->l = q->l->next;
	}
	
	pthread_cond_signal(&(q->cv));
	pthread_mutex_unlock(&(q->mutex));
	return 1;
}

/*La funzione rimuove in testa*/
queue *dequeue(synqueue *q)
{
	queue *tmp;
	
	if(!q) return NULL;
	pthread_mutex_lock(&(q->mutex));
	
	while(isempty(q))
		pthread_cond_wait(&(q->cv), &(q->mutex));
	
	tmp = q->f;
	
	if(q->f)
		q->f = q->f->next;
	else 
		q->f = q->l = NULL;
	
	pthread_mutex_unlock(&(q->mutex));
	
	return tmp;
}

int isempty(synqueue *q)
{
	if(!q || !(q->f)) return 1;
	else return 0;
}
