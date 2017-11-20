#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "wator.h"
#include "visualizer.h"

/**
	@name Macro dei parametri per la simulazione e costanti
	Set di macro con i valori di default per la simulazione e le costanti */
/**@{*/
#define LOG_PATH "./visualizer.log"  /**< Macro con il path del file di log*/
#define SOCK_PATH "./tmp/visual.sck" /**< Macro con il path del file socket*/
#define MAX_STR_LEN 255 /**< Macro con la grandezza delle stringhe statiche usate nel programma */
/**@}*/

/**
	@name Macro per la gestione degli errori e dei log
	Set di macro che semplificano la gestione degli errori e delle anomalie */
/**@{*/
#define TESTMIN(cond,fun,str) {if(cond<0){alert(fun, str);exit(1);}}
#define TESTNOT(cond,fun,str) {if(!cond){alert(fun, str);exit(1);}}
#define FAILWITH(fun,str) {alert(fun, str);exit(1);}
/**@}*/

/**
	@name Routine di chiusura
	Funzione di routine per la gestione della chiusura controllata dell'applicazione*/
void exit_ro() { unlink(SOCK_PATH); }

/**
 * Tabella dei simboli per la decompressione dei codici trasmessi dal processo wator
 * */
char conversion_table[9][2] = { "WW", "WS", "WF", "SS", "SW", "SF", "FF", "FW", "FS" };
 
/* 
 * INIZIO CODICE 
 * */
int main(int argc, char **argv)
{
	struct sockaddr_un conn;
	struct sigaction sigstr;
	int col, row, raw, interrupt = 0, msock, ssock;
	char *strmatrix = NULL, *strzipped = NULL;
	FILE *stream;

	atexit(exit_ro);
	conn.sun_family = AF_UNIX;
	memcpy(conn.sun_path, SOCK_PATH, 108);
	memset(&sigstr, 0, sizeof(sigstr));
	sigstr.sa_handler = SIG_IGN;
	
	/*
	 * GESTIONE STREAM OUTPUT
	 * */
	if(argc == 2)
	{ TESTNOT((stream = fopen(argv[1], "w")), "fopen", strerror(errno)); }
	else stream = stdout;
	
	/*
	 * GESTIONE SOCKET/SEGNALI
	 * */
	TESTMIN(sigaction(SIGINT, &sigstr, 0), "sigaction", strerror(errno));
	TESTMIN(sigaction(SIGTERM, &sigstr,0), "sigaction", strerror(errno));
	
	TESTMIN((msock = socket(AF_UNIX, SOCK_STREAM, 0)), "socket", strerror(errno));
	TESTMIN(bind(msock,(struct sockaddr *)&conn, sizeof(conn)), "bind", strerror(errno));
	TESTMIN(listen(msock, 5), "listen", strerror(errno));
	
	/*
	 * GESTIONE POOL CONNESSIONI
	 * */

	while(!interrupt)
	{
		int zippedsize, matrixsize;
		TESTMIN((ssock = accept(msock, NULL, 0)), "accept", strerror(errno));
		
		/*
		 * RICEZIONE DATI STRUTTURA PIANETA
		 * */
		if(read(ssock, &raw, sizeof(raw)) < sizeof(raw))
			FAILWITH("read", "numero di byte non corretto");
			
		/*
		 * ELABORAZIONE STRUTTURA PIANETA
		 * */
		if(raw)
		{
			TESTNOT(parseDimensions(raw, &col, &row), "parseDimensions", "errore nel parsing dei dati");	
			matrixsize = (col * row);
			TESTNOT((strmatrix = (char *)malloc(sizeof(char) * (matrixsize + 1))), "malloc", strerror(errno));
			strmatrix[matrixsize] = '\0';
			
			zippedsize = ((col * row)/2) + ((col*row)%2);
			TESTNOT((strzipped = (char *)malloc(sizeof(char) * (zippedsize + 1))), "malloc", strerror(errno));
			strzipped[zippedsize] = '\0';
			
			/*
			* RICEZIONE PIANETA
			* */
			if(read(ssock, strzipped, sizeof(char)*(col * row)) < (sizeof(char)*(col * row) / 2 + ((col*row)%2)))
				FAILWITH("read", "numero di byte non corretto");
			
			/*
			* STAMPA PIANETA
			* */
			strmatrix = unzipit(strzipped);
			TESTMIN(printmatrix(strmatrix, stream, col, row), "printmatrix", strerror(errno));
			rewind(stream);
			free_chunk((void **) &strmatrix);
			free_chunk((void **) &strzipped);
		}
		else interrupt = 1;
		close(ssock);
	}
	close(msock);
	exit(0);
}

/*
 * FUNZIONI INTERNE
 * */
 
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
		buff = (char *) malloc(strlen(nowstring) + strlen(string) + strlen(func) + 10);
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
 * Funzione per il parsing delle informazioni sulle dimensioni della matrice che si sta per ricevere
 * 
 * \param raw valore che incapsula le dimensioni della matrice
 * \param col puntatore al numero di colonne che verranno individuate
 * \param row puntatore al numero di righe che verranno individuate
 * 
 * \retval ritorna 0 se il numero da strutturare è non valido
 * \retval ritorna 1 altrimenti
 * */
int parseDimensions(int raw, int *col, int *row)
{
	int index, len;
	
	if(raw < 0)
		return 0;
	
	len = digitsCount(raw);
	
	index = raw / pow(10, (len - 1));
	raw = raw % (int)pow(10, (len - 1));
	
	*col = raw / pow(10, (len - 1 - index));
	*row = raw % (int)pow(10, (len -1 - index));
	
	return 1;
}

/**
 * Funzione che dato un intero, ne calcola il numero di cifre. Viene utilizzato nella funzione structRawVal
 * 
 * \param n numero di cui contare le cifre
 * 
 * \retval numero di cifre
 *
 * @see parseDimensions
 * */
int digitsCount(int n)
{
	return (int) log10(n) + 1;
}

/**
 * Funzione che effettua la stampa del pianeta (sotto forma di stringa) sullo stream passato come parametro
 * 
 * \param string pianeta sotto forma di stringa, ricevuto direttamente dal processo wator
 * \param stream stream su cui si vuole effettuare la stampa
 * \param col numero di colonne della matrice
 * \param row numero di righe della matrice
 * 
 * \retval -1 in caso di errore
 * \retval 1 in caso di successo
 * */
int printmatrix(char *string, FILE *stream, int col, int row)
{
	planet_t *tmp;
	int i, j, k = 0;
	
	if(!(tmp = new_planet(row, col))) return -1;
	
	for(i = 0; i < row; i++)
		for(j = 0; j < col && k < strlen(string); j++, k++)
			tmp->w[i][j] = char_to_cell(string[k]);
	
	if(print_planet(stream, tmp) < 0) return -1;
	free_planet(tmp);
	
	return 1;
}

/**
 * Funzione che si occupa della decompressione dei dati provenienti dal processo wator
 * 
 * \param string stringa da decoprimere
 * \retval restituisce la stringa decompressa o determina la chiusura del processo in caso di errore
 * */
char *unzipit(char *string)
{
	char * new;
	int i, k = 0;
	
	TESTNOT((new = (char *) malloc(sizeof(char) * (2*strlen(string)) + 2)), "malloc", strerror(errno));

	for(i = 0; i < strlen(string); i++)
	{
		new[k]   = conversion_table[string[i] - '0'][0];
		new[k+1] = conversion_table[string[i] - '0'][1];
		k+=2;
	}
	
	new[k] = '\0';
	
	return new;
}
