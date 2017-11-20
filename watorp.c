#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include "watorp.h"
/*#include "synqueue.h"
#include "wator.h"*/
 
/**
	@name Macro dei parametri per la simulazione e costanti
	Set di macro con i valori di default per la simulazione e le costanti*/
/**@{*/
#define SOCK_PATH "./tmp/visual.sck" /**< Macro con il path del file socket*/
#define LOG_PATH "./wator.log" /**< Macro con il path del file di log*/
#define CHECK_PATH "./wator.check" /**< Macro con il path del file di check*/
#define VIS_PATH "./visualizer" /**< Macro con il path dell'eseguibile del processo visualizer*/
#define NWORK_DEF 4 /**< Macro con numero di worker di default*/
#define CHRON_DEF 1 /**< Macro con numero di chronon di default dopo i quali stampare la matrice */
#define MAX_STR_LEN 255 /**< Macro con la grandezza delle stringhe statiche usate nel programma */
#define K 3 /**< Macro del valore delle ascisse delle sottomatrici da individuare*/
#define N 3 /**< Macro del valore delle ordinate delle sottomatrici da individuare */
#define W_FILENAME "wator_worker_" /**< Macro con la stringa statica del path del file che deve creare ogni worker in esecuzione */
/**@}*/

/**
	@name Macro per la gestione degli errori e dei log
	Set di macro che semplifica */
/**@{*/
#define TESTMIN(cond,fun,str) {if(cond<0){alert(fun, str);exit(1);}}
#define TESTNOT(cond,fun,str) {if(!cond){alert(fun, str);exit(1);}}
#define FAILWITH(fun,str) {alert(fun, str);exit(1);}
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
/**@}*/


/* 
 * VARIABILI GLOBALI 
 * */
int sock, valpool_size, nchronon, nworker, **seen;
char conversion_table[9][2] = { "WW", "WS", "WF", "SS", "SW", "SF", "FF", "FW", "FS" }; /**< Tabella statica utilizzata nella conversione durante la compressione e decompressione dei dati tramessi tra wator e visualizer */
wator_t *world;
volatile int sigint_arr = 0, sigusr1_arr = 0;
square_t *valpool;
synqueue workers_queue; /**< CODA DISPATCHER->WORKERS*/
synqueue collector_queue; /**< CODA WORKERS->COLLECTOR*/
synqueue dispatcher_queue; /**< CODA COLLECTOR->DISPATCHER*/
struct sigaction sigstruct;
struct sockaddr_un conn;
pthread_mutex_t **mutexmatrix;

/**
	@name Routine di segnali
	 Seti di funzioni di routine utilizzate dalla gestione dei segnali */
/**@{*/
void sigint_routine(int sig)
{
	sigint_arr = 1;
}

void sigusr1_routine(int sig)
{
	sigusr1_arr = 1;
}
/**@}*/

/* 
 * INIZIO CODICE 
 * */
int main(int argc, char **argv)
{
	/*
	 * VARIABILI LOCALI
	 * */
	int pid, stat;
	char planetfile[MAX_STR_LEN] = {0}, dumpfile[MAX_STR_LEN] = {0};
	pthread_t dispatcher, collector, *workers;
	
	TESTNOT(opt(&nworker, &nchronon, dumpfile, planetfile, argv, argc), "opt", strerror(errno));
	pid = fork();
	
	switch(pid)
	{
		int i, *pt = NULL;
		
		case -1:
			FAILWITH("fork", strerror(errno));
			break;
		case 0:
			if(!strlen(dumpfile)){ TESTMIN(execl(VIS_PATH, "visualizer", (char *)NULL), "execl", strerror(errno)); }
			else { TESTMIN(execl(VIS_PATH, "visualizer", dumpfile, (char *)NULL), "execl", strerror(errno)); }
			break;
		default:
			/*Gestione SIGINT/SIGTERM*/
			memset(&sigstruct, 0, sizeof(sigstruct));
			sigstruct.sa_handler = sigint_routine;
			sigstruct.sa_flags = SA_RESTART;
			TESTMIN(sigaction(SIGINT, &sigstruct, 0), "sigaction", strerror(errno));
			TESTMIN(sigaction(SIGTERM, &sigstruct, 0), "sigaction", strerror(errno));
			
			/*Gestione SIGUSR1*/
			sigstruct.sa_handler = sigusr1_routine;
			TESTMIN(sigaction(SIGUSR1, &sigstruct, 0), "sigaction", strerror(errno));
			
			/*Inizializzazione del pianeta*/
			TESTNOT((world = new_wator(planetfile)), "new_wator", strerror(errno))
			
			/*Inizializzazione della matrice di mutex*/
			TESTNOT((mutexmatrix = initmutexmatrix(world->plan->nrow, world->plan->ncol)), "initmutexmatrix", strerror(errno));
			
			/*Inizializzazione della matrice seen*/
			TESTNOT((seen = initseenmatrix(world->plan->nrow, world->plan->ncol)), "initseenmatrix", strerror(errno));
			
			
			/*Inizializzazione*/
			conn.sun_family = AF_UNIX;
			memcpy(conn.sun_path, SOCK_PATH, 108);
			
			/*Inizializzazione delle code per la comunicazione*/
			initqueue(&workers_queue);
			initqueue(&dispatcher_queue);
			initqueue(&collector_queue);
			initpoolval(&valpool, world->plan->nrow, world->plan->ncol, K, N);
			fillpoolval(valpool, world->plan->nrow, world->plan->ncol, K, N);
			valpool_size = countpoints(world->plan->nrow, world->plan->ncol, K, N);
			
			/*Inizializzazione array di thread worker*/
			TESTNOT((workers = (pthread_t *) ALLOC(pthread_t, nworker)), "malloc", strerror(errno));
			
			#ifdef DEBUG
				printf("LANCIO %d THREAD WORKER\n", nworker);
			#endif
			
			/*Si effettua il push del primo argomento per forzare l'avvio della struttura farm*/
			TESTNOT(enqueue(&dispatcher_queue, &nworker), "enqueue", "Errore nella enqueue");
			
			/*Creazione e lancio dei thread*/
			pthread_create(&dispatcher, NULL, &dispatcher_main, NULL);
			pthread_create(&collector, NULL, &collector_main, NULL);
			for(i = 0; i < nworker; i++)
			{
				TESTNOT((pt = (int *) ALLOC(sizeof(int), 1)), "malloc", strerror(errno));
				*pt = i;
				pthread_create(&workers[i], NULL, &workers_main, (void *)pt);
			}
				
			/*Attesa della terminazione dei thread e del visualizer*/
			pthread_join(dispatcher, NULL);
			for(i = 0; i < nworker; i++)
				pthread_join(workers[i], NULL);
			pthread_join(collector, NULL);
			waitpid(pid, &stat, 0);
			
			/*Vengono liberate tutte le risorse nello heap utilizzate durante l'aggiornamento*/
			free_matrix((void **)mutexmatrix, world->plan->nrow);
			free_matrix((void **)seen, world->plan->nrow);
			free_wator(world);
			free_chunk((void **) &valpool);
			free_chunk((void **) &workers);
			break; 
	}
	return 0;
}

/**
 * Routine principale per il thread collector
 * \param arg argomento fittizio
 * \retval NULL
 * */
void *collector_main(void *arg)
{
	queue *info = NULL;
	int raw, t, worker_jobs_gotten = 0, chronon_coll_count = 0, keep = 1;
	char *string = NULL, *zipped = NULL;
	
	#ifdef DEBUG
		printf("NASCE IL COLLECTOR\n");
	#endif
	
	
	/*Ciclo degli eventi*/
	while(keep)
	{
		/*Attesa fino a poter prelevare un elemento dalla coda degli eventi riempita dai worker*/
		TESTNOT((info = dequeue(&collector_queue)), "dequeue", "Errore nella dequeue");

		/*Si tiene conto del numero di estrazioni effettuate dalla coda e, se queste sono pari al numero di worker, sicuramente questi hanno
		 * terminato l'elaborazione della matrice*/
		worker_jobs_gotten++;
		
		#ifdef DEBUG
			printf("NUMERO DI WORKER RICEVUTO: %d SU %d\n", worker_jobs_gotten, valpool_size);
		#endif
		if(!(worker_jobs_gotten % valpool_size))
		{
			/*Si aggiornano i valori relativi al pianeta solo quando tutti i worker completano l'aggiornamento*/
			world->nf = fish_count(world->plan);
			world->ns = shark_count(world->plan);
			world->chronon = ++chronon_coll_count;
			if(!(chronon_coll_count % nchronon))
			{	
				/*Creazione del canale di comunicazione su socket e trasmissione dei dati al visualizer (compressione)*/
				TESTMIN((sock = socket(AF_UNIX, SOCK_STREAM, 0)), "socket", strerror(errno));
				
				for(t = 0; connect(sock, (struct sockaddr *)&conn, sizeof(conn)) < 0 && t < 10; t++)
					if(errno != ECONNREFUSED && errno != ENOENT)
						{ FAILWITH("connect", strerror(errno)); }
					else sleep(1);
				
				TESTNOT((raw = structRawVal(world->plan->ncol, world->plan->nrow)), "structRawVal", "valore zero");
				TESTNOT((string = matrixstring(world->plan->w, world->plan->nrow, world->plan->ncol)), "matrixstring", "errore nella creazione della stringa");
				zipped = zipit(string);
				TESTMIN(write(sock, &raw, sizeof(int)), "write raw", strerror(errno));
				TESTMIN(write(sock, zipped, strlen(zipped)), "write data", strerror(errno));
				close(sock);
			}
			
			/*Controllo della presenza dei segnali ricevuti durante l'esecuzione della struttura a farm*/
			if(sigusr1_arr)
			{	TESTMIN(checkout(world), "checkout", strerror(errno)); sigusr1_arr = 0;}
			
			/*Pulisco la matrice dtime*/
			cleanmatrix(seen, world->plan->nrow, world->plan->ncol);
			
			/*Se l'informazione è NULL, allora è il momento di far crollare l'architettura terminando, altrimenti si effettua il push di un'informazione
			 * casuale diversa dal valore nullo*/
			keep = !sigint_arr;
			if ( keep )
				enqueue(&dispatcher_queue, &worker_jobs_gotten);
		}
		
		free_chunk((void **)&info);
		free_chunk((void **)&string);
		free_chunk((void **)&zipped);
	}
	
	#ifdef DEBUG
		printf("MUORE IL COLLECTOR\n");
	#endif
	
	/*Chiusura del visualizer e terminazione*/
	TESTMIN(killserver((struct sockaddr *)&conn), "killserver", strerror(errno));
	enqueue(&dispatcher_queue, NULL);
	return NULL;
}

/**
 * Routine principale per i thread worker
 * 
 * \param id puntatore al wid del thread che lo riceve
 * \retval NULL 
 * */
void *workers_main(void *id)
{
	int wid = *((int *)id), fd;
	queue *node = NULL;
	char filename[MAX_STR_LEN] = {0};
	
	#ifdef DEBUG
		square_t *s = NULL;
		printf("NASCE IL WORKER %d\n", wid);
	#endif
	
	
	/*Gesione file*/
	snprintf(filename, MAX_STR_LEN, "%s%d", W_FILENAME, wid);
	TESTNOT((fd = open(filename, O_WRONLY|O_APPEND|O_CREAT, 0777)), "workers_main", strerror(errno));
	close(fd);
	free_chunk((void **)&id);
	
	/*Ciclo degli eventi*/
	while(1)
	{
		/*Attesa fino a poter estrarre un elemento dalla coda degli eventi provenienti dal collector*/
		TESTNOT((node = dequeue(&workers_queue)), "dequeue", "Errore nella dequeue");
		
		/*Se l'informazione ha valore NULL, informo il collector e muoio*/
		if(!(node->info))
		{
			free_chunk((void **)&node);
			#ifdef DEBUG
				printf("MUORE IL WORKER %d\n", wid);
			#endif
			return NULL;
		}
		
		#ifdef DEBUG
			s = (square_t *)node->info;
			printf("Il worker %d elabora la sottomatrice (%d,%d) | (%d,%d)\n", wid, s->left.x, s->left.y, s->right.x, s->right.y);
		#endif
		
		/*SEZIONE CRITICA*/
		TESTMIN(safely_work(world, node->info, mutexmatrix, seen), "safely_work", strerror(errno));
		
		/*Si effettua la push del valore wid del thread per poter far procedere il flusso d'esecuzione*/
		TESTNOT(enqueue(&collector_queue, &wid), "workers_main", "Errore nella enqueue");
		free_chunk((void **)&node);
	}
}


/**
 * Routine principale per il thread dispatcher
 * 
 * \param arg argomento fittizio
 * 
 * \retval NULL
 * */
void *dispatcher_main(void *arg)
{
	int i;
	queue *resp = NULL;

	/*Ciclo degli eventi*/
	while(1)
	{
		/*Si preleva il messaggio del collector*/
		TESTNOT((resp = dequeue(&dispatcher_queue)), "dequeue", "Errore nella dequeue");
		
		#ifdef DEBUG
			printf("NASCE IL DISPATCHER\n");
		#endif
	
		/*Si analizza il contenuto del messaggio del collector*/
		if(!(resp->info))
		{
			for(i = 0; i < nworker; i++)
				if(!enqueue(&workers_queue, NULL))
					FAILWITH("dispatcher_main", "Errore nelle enqueue");
			#ifdef DEBUG
				printf("MUORE IL DISPATCHER\n");
			#endif
			free_chunk((void **)&resp);
			return NULL;
		}
		else fillqueue(valpool, valpool_size, &workers_queue);
		free_chunk((void **)&resp);
	}
}

/*
 * FUNZIONI INTERNE
 * */
 
 /**
 * Funzione che gestisce i parametri dati in input per la simulazione
 * 
 * \param w variabile che ospiterà il numero di chronon che distingueranno le visualizzazioni
 * \param c variabile che ospiterà il numero di worker che si occuperanno dell'aggiornamento della matrice
 * \param d variabile che ospiterà il nome del file su cui verrà stampato il pianeta
 * \param f variabile che ospiterà in nome del file del pianeta
 * \param argv argomenti del main
 * \param numero argomenti del main
 * 
 * \retval 0 in caso di errore
 * \retval 1 in caso di successo
 * */
int opt(int *w, int *c, char *d, char *f, char **argv, int argc)
{
	int i, pos = 0;
	
	/*Vengono inizializzati le variabili con valori di default*/
	*w = NWORK_DEF;
	*c = CHRON_DEF;
	
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-f"))
		{
			if(argv[i+1])
			{
				i++;
				strncpy(d, argv[i], 255);
			}
			else
			{
				errno = EINVAL;
				return 0;
			}
		}
		else if(!strcmp(argv[i], "-v"))
		{
			if(argv[i+1])
			{
				i++;
				*w = atoi(argv[i]);
			}
			else
			{
				errno = EINVAL;
				return 0;
			}
		}
		else if(!strcmp(argv[i], "-n"))
		{
			if(argv[i+1])
			{
				i++;
				*c = atoi(argv[i]);
			}
		}
		else if(canread(argv[i]))
		{
			strncpy(f, argv[i], strlen(argv[i]));
			pos = 1;
		}
	}
	
	if(pos && canread(CONFIGURATION_FILE)) return 1;
	else 
	{
		errno = EINVAL;
		return 0;
	}
}

/**
 * Funzione che verifica la possibilià di leggere il file il cui nome viene passato come parametro
 * 
 * \param path nome del file
 * 
 * \retval 0 se non è possibile leggere il file path
 * \retval 1 se è possibile leggere il file path
 * */
int canread(const char *path)
{
	struct stat b;
	if(!stat(path, &b)) return 1;
	else return 0;
}

/**
 * Funzione che effettua il log su file in caso di errore
 * 
 * \param func nome della funzione a cui attribuire il log
 * \param string stringa da loggare
 * */
void alert(char *func, char *string)
{
	int fd;
	char *nowstring = getTime(), *buff = NULL;
	nowstring[strlen(nowstring) -1] = '\0';
	if((fd = open(LOG_PATH, O_WRONLY|O_APPEND|O_CREAT, 0777)) != -1)
	{
		buff = (char *) ALLOC(char, strlen(nowstring) + strlen(string) + strlen(func) + 10);
		sprintf(buff, "[%s]%s: %s\n", nowstring, func, string);
		write(fd, buff, strlen(buff));
	}
	free_chunk((void **)&buff);
	close(fd);
}

/**
 * Funzione che restituisce la stringa della data attuale
 * */
char *getTime ()
{
    time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	return asctime (timeinfo);
}

/**
 * Funzione che costruisce un intero il cui valore serve a codificare le grandezze della matrice del pianeta. Il valore assume un ruolo importante
 * per la trasmissione minima dei dati tra il processo worker e il visualizer
 * 
 * \param col numero di colonne della matrice del pianeta da visualizzare
 * \param row numero di righe della matrice del pianeta da visualizzare
 * 
 * \retval valore codificato dalle grandezze della matrice
 * */
int structRawVal(int col, int row)
{
	int ccol = digitsCount(col), crow = digitsCount(row);
	return ccol*(pow(10,(ccol+crow))) + col*(pow(10,crow)) + row;
}

/**
 * Funzione che dato un intero, ne calcola il numero di cifre. Viene utilizzato nella funzione structRawVal
 * 
 * \param n numero di cui contare le cifre
 * 
 * \retval numero di cifre
 *
 * @see structRawVal
 * */
int digitsCount(int n)
{
	return (int) log10(n) + 1;
}

/**
 * Funzione che costruisce una matrice di caratteri partendo dalla matrice di celle e dalle sue dimensioni
 * 
 * \param matrix matrice di celle
 * \param row numero di righe della matrice
 * \param col numero di colonne della matrice
 * 
 * \retval matrice di caratteri
 * */
char *matrixstring(cell_t **matrix, int row, int col)
{
	char *ret;
	int i, j, k = 0;
	
	TESTNOT((ret = (char *) ALLOC(sizeof(char), (row * col+1))), "[matrixstring]malloc", strerror(errno));
	
	for(i = 0; i < row; i++)
		for(j = 0; j < col; j++, k++)
			ret[k] = cell_to_char(matrix[i][j]);
	
	ret[row*col] = '\0';
	return ret;
}

/**
 * Funzione che effettua la compressione dei dati da inviare al visualizer. Si cerca in questo modo di ridurre il numero di byte da inviare
 * 
 * \param string stringa di caratteri da comprimere
 * 
 * \retval stringa compressa di caratteri
 * */
char *zipit(char *string)
{
	char *new = NULL;
	int k, i, z = 0;
	
	#ifdef DEBUG
		printf("COMPRESSIONE DEI DATI\n");
	#endif
	
	TESTNOT((new = (char *) ALLOC(char, strlen(string) + 1)), "[zipit]malloc", strerror(errno));
	
	for(k = 0; k + 1 < strlen(string);k+=2)
		for(i = 0; i < 9; i++)
			if(string[k] == conversion_table[i][0] && string[k+1] == conversion_table[i][1])
			{
				new[z] = i + '0';
				z++;
			}
			
	for(; k < strlen(string); k++, z++)
		new[z] = string[k];
	
	new[z] = '\0';
	
	return new;
	
}

/** Funzione che informa il processo visualizer che dovrà morire
 * 
 * \param conn struttura con i dati della connessione tra wator e visualizer
 * 
 * \retval 0 in caso di errore
 * \retval 1 in caso di riuscita
 * */
int killserver(struct sockaddr *conn)
{
	int t, tsock;
	
	#ifdef DEBUG
		printf("OMICIDIO DEL VISUALIZER\n");
	#endif
	
	if((tsock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
				
	for(t = 0; connect(tsock, conn, sizeof(struct sockaddr_un)) < 0 && t < 5; t++)
		if(errno != ECONNREFUSED && errno != ENOENT)
			return -1;
	
	t = 0;
	if(write(tsock, &t, sizeof(t)) < 0)
		return -1;
	
	close(tsock);
	return 1;
}

/**
 * Funzione che si occupa di effettuare il checkout del pianeta su file. Viene utilizzata per gestire il segnale SIGUSR1
 * 
 * \param world struttura contenente i dati del pianeta
 * 
 * \retval -1 in caso di errore
 * \retval 0 in caso di riuscita
 * */
int checkout(wator_t *world)
{
	FILE *stream;
	
	#ifdef DEBUG
		printf("CHECKOUT DEL PIANETA EFFETTUATO\n");
	#endif
	
	if(!(stream = fopen(CHECK_PATH, "w"))) return -1;
	if(print_planet(stream, world->plan) < 0) return -1;
	fclose(stream);
	return 1;
}

/**
 * Funzione che si occupa di inizializzare il pool di valori rappresentanti le sottomatrici da aggiornare.
 * I perimetri delle sottomatrici vengono calcolate una volta sola per esecuzione perchè siamo sicuri non cambino a run-time.
 * 
 * \param val puntatore alla lista di valori
 * \param dimx dimensione di x totale della matrice
 * \param dimy dimensione di y totale della matrice
 * \param parx dimensione di x della sottomatrice
 * \param pary dimensione di y della sottomatrice
 * 
 * \retval 0 in caso di errore
 * \retval n numero di sottomatrici individuabili in caso di successo
 * */
int initpoolval(square_t **val, int dimx, int dimy, int parx, int pary)
{
	int n = countpoints(dimx, dimy, parx, pary);
	if(!n) return 0;
	if(!((*val) = (square_t *)malloc(sizeof(square_t) * n))) return 0;
	else return n;
}

/**
 * Procedura che riempie la lista delle sottomatrici individuandole dalla matrice principale.
 * 
 * \param val lista dei valori
 * \param dimx dimensione di x totale della matrice
 * \param dimy dimensione di y totale della matrice
 * \param parx dimensione di x della sottomatrice
 * \param pary dimensione di y della sottomatrice
 * */
void fillpoolval(square_t *val, int dimx, int dimy, int parx, int pary)
{
	int i, j, k=0;
	
	#ifdef DEBUG
		printf("INDIVIDUATI I SEGUENTI PUNTI DELLE DIAGONALI:\n");
	#endif
	
	for(i = 0; i < dimy ; i += pary)
	{
		for(j = 0; j < dimx ; j += parx)
		{
			val[k].left.x = j; /*X1*/
			val[k].left.y = i; /*Y1*/
			val[k].right.x = MIN((j + parx-1), (dimx - 1)); /*X2*/
			val[k].right.y = MIN((i + pary-1), (dimy - 1)); /*Y2*/
			#ifdef DEBUG
				printf("(%d,%d) (%d,%d)\n", val[k].left.x, val[k].left.y, val[k].right.x, val[k].right.y);
			#endif
			k++;
		}
	}
}

/**
 * Funzione che si occupa di effettuare l'inserimento dei valori delle coordinate delle aree delle sottomatrici
 * nella coda sincronizzata data in input
 * 
 * \param val lista delle dimensioni delle sottomatrici individuate
 * \param size numero di elementi presenti nel pool delle sottomatrici
 * \param q puntatore alla coda sincronizzata in cui effettuare l'inserimento
 * 
 * \retval 0 in caso di errore
 * \retval 1 in caso di successo
 * */
int fillqueue(square_t *val, int size, synqueue *q)
{
	int i;
	
	#ifdef DEBUG
		printf("PUSH DEI SEGUENTI VALORI NELLA CODA DI WORKERS: \n");
	#endif
	
	for(i = 0; i < size; i++)
	{
		#ifdef DEBUG
			printf("%d-->(%d,%d) (%d,%d)\n",i, val[i].left.x, val[i].left.y, val[i].right.x, val[i].right.y);
		#endif
		if(!enqueue(q, &val[i])) return 0; 
	}
	return 1;
}

/**
 * Funzione che calcola in numero di sottomatrici individuabili su quella principale
 * 
 * \param dimx dimensione di x totale della matrice
 * \param dimy dimensione di y totale della matrice
 * \param parx dimensione di x della sottomatrice
 * \param pary dimensione di y della sottomatrice
 * 
 * \retval numero di sottomatrici individuabili
 * */
int countpoints(int dimx, int dimy, int parx, int pary)
{
	int modx, mody;
	
	if(dimx <= 0 || dimy <= 0 || parx <= 0 || pary <= 0) return 0;
	
	if((dimx % parx) > 0) modx = 1;
	else modx = 0;
	
	if((dimy % pary) > 0) mody = 1;
	else mody = 0;
	
	return (dimx/parx)*(dimy/pary)+(dimx/parx)*mody+(dimy/pary)*modx + modx*mody;
}

/**
 * Funzione che istanzia e inizializza una matrice di mutex
 * 
 * \param nrow numero di righe della matrice da inizializzare
 * \param ncol numero di colonne della matrice da inizializzare
 * 
 * \retval NULL in caso di errore
 * \retval puntatore alla matrice inizializzata
 * */
pthread_mutex_t **initmutexmatrix(int nrow, int ncol)
{
	int i, k;
	pthread_mutex_t **new = NULL;
	errno = 0;
	
	if(nrow < 0 || ncol < 0){ errno = EINVAL; return NULL; }
	if(!(new = (pthread_mutex_t **)ALLOC(pthread_mutex_t *, nrow))){ printf("CIAO");errno = ENOMEM; return NULL; }
	for(i = 0; i < nrow && !errno; i++)
		if(!(new[i] = (pthread_mutex_t *) ALLOC(pthread_mutex_t, ncol)))
		{
			errno = ENOMEM;
			for(k = i-1; k >= 0; k--)
				FREEROW(new[k]);
		}
	
	if(errno)
	{ FREECOL(new); return NULL; }
	
	for(i = 0; i < nrow; i++)
		for(k = 0; k < ncol; k++)
			pthread_mutex_init(&new[i][k], NULL);
	
	return new;
}

/**
 * Funzione che istanzia e inizializza una matrice di interi.
 * La matrice per cui viene utilizzata la funzione viene utilizzata per tenere traccia degli esseri di cui ignorare i movimenti.
 * 
 * \param nrow numero di righe della matrice
 * \param ncol numero di colonne della matrice
 * 
 * \retval NULL in caso di errore
 * \retval puntatore alla matrice in caso di successo
 * */
int **initseenmatrix(int nrow, int ncol)
{
	int i, k;
	int **new = NULL;
	errno = 0;
	
	if(nrow < 0 || ncol < 0){ errno = EINVAL; return NULL; }
	if(!(new = ALLOC(int *, nrow))){ errno = ENOMEM; return NULL; }
	
	for(i = 0; i < nrow && !errno; i++)
		if(!(new[i] = (int *) ALLOC(int, ncol)))
		{
			errno = ENOMEM;
			for(k = i-1; k >= 0; k--)
				FREEROW(new[k]);
		}
	if(errno)
	{ FREECOL(new); return NULL; }
	
	cleanmatrix(new, nrow, ncol);
	return new;
}

/**
 * Procedura che libera la memoria allocata dalla matrice passata come argomento
 * 
 * \param m puntatore alla matrice multidimensionale
 * \param row numero di righe della matrice
 * */
void free_matrix(void **m, int row)
{
	int i;
	for(i = 0; i < row; i++)
		if(m[i]) FREEROW(&m[i]);
	
	if(m) FREECOL(&m);
}

/**
 * Procedura che azzera la matrice di interi passata come argomento
 * 
 * \param m puntatore alla matrice multidimensionale
 * \param row numero di righe della matrice
 * \param col numero di colonne della matrice
 * */
void cleanmatrix(int **m, int row, int col)
{
	int i, k;
	for(i = 0; i < row; i++)
		for(k = 0; k < col; k++)
			m[i][k] = 0;
}
