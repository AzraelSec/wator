#include "wator.h"
#include "synqueue.h"
int opt(int *w, int *c, char *d, char *f, char **argv, int argc);
int canread(const char *path);
void alert(char *func, char *string);
char *getTime ();
int digitsCount(int n);
int structRawVal(int col, int row);
char *matrixstring(cell_t **matrix, int row, int col);
char *zipit(char *string);
int killserver();
int checkout(wator_t *world);
int countpoints(int dimx, int dimy, int parx, int pary);
int initpoolval(square_t **val, int dimx, int dimy, int parx, int pary);
void fillpoolval(square_t *val, int dimx, int dimy, int parx, int pary);
int fillqueue(square_t *val, int size, synqueue *q);
void *dispatcher_main(void *valsize);
void *collector_main(void *arg);
void *workers_main(void *id);
pthread_mutex_t **initmutexmatrix(int nrow, int ncol);
void free_matrix(void **m, int row);
void cleanmatrix(int **m, int row, int col);
int **initseenmatrix(int nrow, int ncol);
