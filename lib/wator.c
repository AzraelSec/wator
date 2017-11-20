/**
	@file
	\author Federico Gerardi
	\brief File d'implementazione del progetto WATOR
	\ingroup b
	
	Dichiaro che questo codice è in ogni sua parte inventiva dell'autore.
*/

#include "wator.h"

char cell_to_char (cell_t a)
{
	switch(a)
	{
		case WATER:
			return 'W';
			break;
		case SHARK:
			return 'S';
			break;
		case FISH:
			return 'F';
			break;
		default:
			return '?';
		break; 
	}
}

int char_to_cell (char c)
{
	switch(c)
	{
		case 'W':
			return WATER;
			break;
		case 'S':
			return SHARK;
			break;
		case 'F':
			return FISH;
			break;
		default:
			return -1;
		break;
	}
}

planet_t * new_planet (unsigned int nrow, unsigned int ncol)
{
	
	planet_t *newplanet; 
	int i;
	
	errno = 0; /*Setto la variabile errno a zero perchè, avendo valore logico nel seguito della funzione, ho bisogno
				* non contenga valori "sporchi" provenienti da eventuali errori precedenti.*/
	if(nrow <= 0 || ncol <= 0) /*Controllo che i valori di riga e colonna siano positivi, altrimenti sollevo un errore.*/
		return (planet_t*)error(EINVAL);
	
	if(!(newplanet = (planet_t *) ALLOC(planet_t,1)))/*Controllo che la prima allocazione abbia successo.*/
		error(ENOMEM);
	
	/*Assegno i valori di nrow e ncol ai relativi campi della struttura.*/
	newplanet->nrow = nrow;
	newplanet->ncol = ncol;
	
	/*Alloco le matrici contenute nella struttura del pianeta.
	* Prima istanzio un vettore di vettori (ognuno del proprio tipo) e successivamente alloco i vettori puntati dai precedenti.
	* Se si riscontra un problema di allocazione per una malloc(), la funzione dealloca tutto quello precedentemente allocato,
	*  imposta il valore di errno e ritorna NULL.
	*/
	if((newplanet->w = (cell_t **) ALLOC(cell_t *, nrow)) &&
		(newplanet->btime = (int **) ALLOC(int *, nrow))  &&
		(newplanet->dtime = (int **) ALLOC(int *, nrow)))
	{
		for(i = 0; i < nrow && !errno; i++)
			if(!(newplanet->w[i] = (cell_t *) ALLOC(cell_t, ncol)) ||
				!(newplanet->btime[i] = (int *) ALLOC(int, ncol))  ||
				!(newplanet->dtime[i] = (int *) ALLOC(int, ncol)))
			{
				int k;
				errno = ENOMEM;
				for(k = i-1; k >= 0; k--)
					FREEROW(newplanet->w[k]);
			}
	}
	else errno = ENOMEM;
	
	if(errno)
	{
		FREECOL(newplanet->btime);
		FREECOL(newplanet->dtime);
		FREECOL(newplanet->w);
		free_chunk((void **) &newplanet);
	}
	else
	{	/*Se non vengono riscontrati errori di allocazione, si passano in rassegna le matrici, inizializzandole ai valori di default.*/
		int j;
		for(i = 0; i < nrow ; i++)
			for(j = 0; j < ncol; j++)
				{
					newplanet->w[i][j] = WATER;
					newplanet->btime[i][j] = 0;
					newplanet->dtime[i][j] = 0;
				}
	}
	
	return newplanet; /*Viene restituito il riferimento alla struttura allocata e inizializzata.*/
}

void free_planet (planet_t* p)
{
	int i;
	
	if(p) /*Si controlla che il parametro p sia valido e non NULL.*/
	{
		for(i = 0; i < p->nrow; i++) /*Si dealloca ogni colonna dopo la prima.*/
		{
			FREEROW(&p->w[i]);
			FREEROW(&p->dtime[i]);
			FREEROW(&p->btime[i]);
		}
		
		/*Si deallocano i vettori di puntatori delle matrici interne alla struttura del pianeta.*/
		if(p->w) FREECOL(&p->w);
		if(p->dtime) FREECOL(&p->dtime); 
		if(p->btime )FREECOL(&p->btime);
		free_chunk((void **)&p);
	}
}

int print_planet (FILE* f, planet_t* p)
{
	int i, j;
	if(!f) /*Si controlla la validità del puntatore al file del pianeta.*/
	{
		error(EINVAL);
		return -1;
	}
	
	if(!fprintf(f, "%i\n%i\n", p->nrow, p->ncol)) /*Stampo sullo stream di output i dati relativi alle grandezze delle matrici del pianeta.*/
	{
		error(EIO);
		return -1;
	}
	/*Si cicla su ogni cella della matrice e stampo il contenuto.
	* Per ogni valore di cell_t si utilizza un colore differente se lo stream di output coincide con lo stdout*/
	for(i = 0; i < p->nrow; i++)
	{
		for(j = 0; j < p->ncol; j++)
		{
			if(p->w[i][j] == FISH)
			{
				if(f == stdout)
				{
					if(!fprintf(f, ANSI_COLOR_GREEN "%c" ANSI_COLOR_RESET, cell_to_char(p->w[i][j])))
					{
						error(EIO);
						return -1;
					}
				}
				else
					if(!fprintf(f, "%c", cell_to_char(p->w[i][j])))
					{
						error(EIO);
						return -1;
					}
			}
			else if(p->w[i][j] == SHARK)
			{
				if(f == stdout)
				{
					if(!fprintf(f, ANSI_COLOR_RED "%c" ANSI_COLOR_RESET, cell_to_char(p->w[i][j])))
					{
						error(EIO);
						return -1;
					}
				}
				else if(!fprintf(f, "%c", cell_to_char(p->w[i][j])))
					 {
					 	error(EIO);
					 	return -1;	
					 }
			}
			else
			{
				if(f == stdout)
				{
					if(!fprintf(f, ANSI_COLOR_BLUE "%c" ANSI_COLOR_RESET, cell_to_char(p->w[i][j])))
					{
						error(EIO);
						return -1;
					}
				}
				else if(!fprintf(f, "%c", cell_to_char(p->w[i][j])))
					 {
					 	error(EIO);
					 	return -1;
					 }
			}
			if(j < p->ncol-1)
				if(!fprintf(f, " "))
					return -1;
		}
		
		if(!fprintf(f, "\n"))
			return -1;
	}
	
	return 0;
}

planet_t* load_planet (FILE* f)
{
	planet_t *newplanet;
	int nrow = 0, ncol = 0, i, j;
	char c;
	
	/*Si prendono i valori interi relativi al numero di righe e colonne che costituiranno la struttura del pianeta.*/
	if(fscanf(f, "%i\n%i\n", &nrow, &ncol) != 2)
		return error(ERANGE);
	
	if(nrow < 0 || ncol < 0) /*Si controlla che il valore del numero di colonne e di righe non sia negativo.*/
 		return error(ERANGE);
	
	if(!(newplanet = new_planet(nrow, ncol))) /*Si alloca lo spazio per la struttura planet_t e la inizializzo.*/
		return error(ERANGE);
		
	for(i = 0; i < nrow; i++) /*Si effettua una scansione dei caratteri della matrice tenendo presente i range inseriti in precedenza.*/
		for(j = 0; j < ncol; j++)
			if(!fscanf(f, "%c ", &c))
				return error(EIO);
			else
				newplanet->w[i][j] = char_to_cell(c);/*Si effettua la coversione da tipo cell_t a char.*/
	
	return newplanet;
}

wator_t* new_wator (char* fileplan)
{
	FILE *f = NULL;
	wator_t *wator = NULL;
	int i;
	
	if(!(f = fopen(CONFIGURATION_FILE, "r"))) /*Si accede la risorsa del file di configuazione per il pianeta Wator in sola lettura.*/
		return error(EBADF);
	
	if(!(wator = (wator_t *) ALLOC(wator_t, 1))) /*Si alloca memoria per contenere la struttura per la rappresentazione del pianeta.*/
		return error(ENOMEM);
	
	/*Si accede in lettura al file per leggere le informazioni riguardanti i dati di riproduzione degli esseri.*/
	if(fscanf(f, "sd %i\nsb %i\n fb %i", &wator->sd, &wator->sb, &wator->fb) != 3)
	{
		free_chunk((void **) &wator);
		return error(EINVAL);
	}
	
	/*Le informazioni riguardanti l'esecuzione della simulazione vengono settati a valore di default per poi essere calcolati a runtime.*/
	wator->nf = 0;
	wator->ns = 0;
	wator->nwork = 0;
	wator->chronon = 0;
	
	if(f) fclose(f); /*Si effettua la chiusura della risorsa in lettura.*/
	
	if(!(f = fopen(fileplan, "r")))/*Si apre il file della struttura del pianeta.*/
	{
		free_chunk((void **) &wator);
		return error(ERANGE);
	}
	
	if(!(wator->plan = load_planet(f))) /*Viene effettuato il parsing della struttura fisica del pianeta ricostruendo le matrici di simulazione.*/
	{
		if(f) fclose(f);
		free_chunk((void **) &wator);
		return NULL;
	}
	else
	{	/*Vengono aggiornati i valori numerici che indicano il numero di pesci e di squali presenti nel pianeta.*/
		for(i = 0; i < wator->plan->nrow; i++)
		{
			int j;
			
			for(j = 0; j < wator->plan->ncol; j++)
				if(wator->plan->w[i][j] == FISH)
					wator->nf++;
				else if(wator->plan->w[i][j] == SHARK)
					wator->ns++;
		}
		
		if(f) fclose(f); /*Si effettua la chiusura della risorsa in lettura.*/
		return wator;
	}
}

void free_wator(wator_t* pw)
{
	if(pw)/*Si controlla che il file descriptor non sia NULL.*/
	{
		if(pw->plan)/*Se il puntatore alla struttura planet_t è valida, viene deallocata la memoria adibita al suo contenimento.*/
			free_planet(pw->plan);
		
		free_chunk((void **) &pw);
	}
}
/*
	Per gestire i movimenti degli squali utilizzo un array di puntatori a funzioni in cui ci sono i riferimenti alle funzioni di movimento.
	Vengono controllate le posizioni adiacenti allo squalo e se ne verifica il contenuto. Per ogni cella, se è occupata da un pesce, viene
	 incrementato il must_indexes_len e l'indice della funzione di movimento viene salvato in must_indexes, se è libera viene invece
	 viene incrementato il valid_indexes_len e viene adottata la stessa logica sull'array valid_indexes, altrimenti non si fa nulla.
	La logica è quella di immagazzinare in must_indexes gli indici delle funzioni che porterebbero lo squalo a mangiare un pesce e di 
	 immagazzinare in valid_indexes gli indici delle funzioni che porterebbero lo squalo in una posizione valida.
	In questo modo, se il must_indexes_len è pari a zero, sono consapevole del fatto che non ci sono pesci nelle vicinanze dello squalo
	 e dunque analizzo le posizioni valide. Se il valid_indexes_len è pari anch'esso a zero, so per deduzione che non ci sono celle valide
	 nelle vicinanze dello squalo.
	Memorizzando i valori degli indici delle funzioni di moviemento in altri array, è molto semplice randomizzare il processo di selezione
	 in caso si trovassero più pesci nelle vicinanze o più posizioni valide.
*/
int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l)
{
	int valid_indexes[4], must_indexes[4]; /*Array contenenti i riferimenti agli indici per le funzioni di movimento.*/
	int valid_indexes_len = 0, must_indexes_len = 0; /*Contatori per il numero di posizioni interessanti.*/
	f_pointer_t functions[4] = { moveUP, moveDOWN, moveLEFT, moveRIGHT };/*Array di puntatori alle funzioni di movimento.*/
	
	/*Si controlla che la correttezza dei valori da elaborare.*/
	if(pw->plan->w[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] != SHARK)
	{
		error(EINVAL);
		return -1;
	}
	
	/*I puntatori alla nuova posizione assumono gli stessi valori della posizione di partenza, così che rimangano
	   riferimenti validi anche nel caso lo squalo non dovesse muoversi.*/
	*k = HERE(x, pw->plan->nrow);
	*l = HERE(y, pw->plan->ncol);
	
	/*Si effettuano i controlli per designare la successiva mossa dello squalo e poi richiamo la funzione di movimento
	   adatta randomizzandone l'indice nell'array delle funzioni.*/
	if(!(must_indexes_len = findIt(FISH, must_indexes, pw->plan, x, y)))
	{
		if(!(valid_indexes_len = findIt(WATER, valid_indexes, pw->plan, x, y)))
			return STOP;
		else
			return (*functions[valid_indexes[rand() % (valid_indexes_len)]])(pw->plan, x, y, k, l);
	}
	else return (*functions[must_indexes[rand() % (must_indexes_len)]])(pw->plan, x, y, k, l); 
}

/*
	Per gestire l'algoritmo di riproduzione degli squali viene utilizzata una logica molto vicina a quella per gestirne i movimenti.
	Viene dunque richiamata la funzione findIt per capire quali siano le posizioni in cui è presente solo acqua e sucessivamente si
	 richiama la funzione createLife indicando il punto in cui creare il nuovo squalo utilizzando i valori calcolati da findIt.
*/
int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l)
{
	int valid_movements[4] = {0};
	int valid_movements_len = 0;

	/*Si controlla che i valori in input non siano scorretti.*/
	if(pw->plan->w[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] != SHARK)
	{
		errno = EINVAL;
		return -1;
	}
	
	*k = HERE(x, pw->plan->nrow);
	*l = HERE(y, pw->plan->ncol);
	
	if(pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] >= pw->sb) /*Si verifica che il valore sb abbia raggiunto la soglia.*/
	{		
		if((valid_movements_len = findIt(WATER, valid_movements, pw->plan, x, y))) /*Rilevo le posizioni valide per ospitare il nuovo essere.*/
		{
			int	rand_index = rand() % valid_movements_len;
			if(!createLife(pw->plan, SHARK, x, y, valid_movements[rand_index], k, l)) /*Creo la nuova creatura.*/
			{
				errno = ESPIPE;
				return -1;
			}
		}
		pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] = 0; /*Dopo aver generato il nuovo essere, si azzera il contatore sb.*/
	}
	else
		pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)]++; /*Se non si è raggiunta la soglia, si incrementa il valore di sb.*/

	if(pw->plan->dtime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] >= pw->sd) /*Si verifica che il valore sd abbia raggiunto la soglia.*/
	{
		pw->plan->w[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] = WATER; /*Uccido lo squalo.*/
		pw->plan->dtime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] = 0;
		return DEAD;
	}
	else
		pw->plan->dtime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)]++; /*Se non si è raggiunta la soglia, si incrementa il contatore.*/
		
	return ALIVE;
}

/*
	La logica implementativa dei movimenti e della riproduzione dei pesci è la stessa utilizzata per gli squali.
	Si è cercato il più possibile di poter riutilizzare i frammenti di codice legati alle funzioni di cui sopra.
*/
int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l)
{
	int valid_indexes[4] = {0}, valid_indexes_len = 0;
	f_pointer_t functions[4] = { moveUP, moveDOWN, moveLEFT, moveRIGHT };

	if(pw->plan->w[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] != FISH) /*Si controlla la validità dei dati in input.*/
	{
		errno = EINVAL;
		return -1;
	}
	
	/*I dati relativi allo spostamento vengono preimpostati.*/
	*k = HERE(x, pw->plan->nrow);
	*l = HERE(y, pw->plan->ncol);
	
	if((valid_indexes_len = findIt(WATER, valid_indexes, pw->plan, x, y)))
		return (*functions[valid_indexes[rand() % valid_indexes_len]])(pw->plan, x, y, k, l);
	else
		return STOP;
}

int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l)
{
	int valid_movements[4] = {0};
	int valid_movements_len = 0;

	if(pw->plan->w[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] != FISH)/*Si controllano i dati in input.*/
	{
		errno = EINVAL;
		return -1;
	}

	*k = HERE(x, pw->plan->nrow);
	*l = HERE(y, pw->plan->ncol);	
	
	if(pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] >= pw->sb)
	{	
		if((valid_movements_len = findIt(WATER, valid_movements, pw->plan, x, y)))
		{
			int	rand_index = rand() % valid_movements_len;
			if(!createLife(pw->plan, FISH, x, y, valid_movements[rand_index], k, l))
			{
				errno = ESPIPE;
				return -1;
			}
		}
		
		pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)] = 0;
	}
	else
		pw->plan->btime[HERE(x, pw->plan->nrow)][HERE(y, pw->plan->ncol)]++;
		
	return 0;
	
}

int fish_count (planet_t* p)
{
	int i, j, nfish = 0;
	if(!p) /*Si controlla che il puntatore non sia nullo.*/
	{
		errno = EINVAL;
		return -1;
	}
	else
		for(i = 0; i < p->nrow; i++) /*Viene percorsa l'intera matrice e si incrementa nfish ogni volta s'incontra un valore FISH.*/
			for(j = 0; j < p->ncol; j++)
				if(p->w[i][j] == FISH)
					nfish++;

	return nfish;
}

/*
	La logica è identica a  quella utilizzata per la funzione fish_count.
*/
int shark_count (planet_t* p)
{
	int i, j, nshark = 0;
	if(!p)
	{
		errno = EINVAL;
		return -1;
	}
	else
		for(i = 0; i < p->nrow; i++)
			for(j = 0; j < p->ncol; j++)
				if(p->w[i][j] == SHARK)
					nshark++;
	
	return nshark;
}

/*
	Poichè all'interno della funzione, vengono applicate le regole sui movimenti e sulla riproduzione rispettando un ordine
	 di scorrimento per righe e poichè ogni essere potrebbe effettuare un movimento verso il basso, c'è il rischio che le
	 alcuni di loro possano essere processati due o più volte, muovendosi o riproducendosi più di una volta a turno.
	Per risolvere questo problema, si sfrutta il campo dtime; in particolare quando si effettua un movimento in (k,l) o
	 una riproduzione causa l'occupazione della stessa posizione, il suo campo dtime viene reso negativo e viene decrementato.
	Quando si incontra una cella con il relativo valore di dtime negativo, si applica il procedimento contrario, ripristinando
	 il suo valore originale.
	La logica è quella di poter "abusare" del campo dtime come flag per capire se una cella dev'essere presa in considerazione
	 o meno.
*/
int update_wator (wator_t * pw)
{
	int i, j, k, l, k_child, l_child;
	
	if(!pw) /*Si controlla la validità del puntatore in input.*/
	{
		errno = EINVAL;
		return -1;
	}
	
	/*Viene percorsa la matrice una prima volta in cerca degli squali e vengono applicate le funzioni shark_rule1 e shark_rule2
	 a ognuno di essi.
	Per evitare comportamenti inattesi, vengono processati prima gli squali e poi i pesci, rispettando l'ordine di applicazione
	 delle regole.*/
	for(i = 0; i < pw->plan->nrow; i++)
	{
		for(j = 0; j < pw->plan->ncol; j++)
		{
			k_child = l_child = -1;
			if(pw->plan->w[i][j] == SHARK)
			{
				if(pw->plan->dtime[i][j] < 0)
					pw->plan->dtime[i][j] = -(pw->plan->dtime[i][j] + 1); /*Si applica l'algoritmo di "flag".*/
				else
				{
					if(shark_rule1(pw, i, j, &k, &l) < 0) return -1; /*Si applicano le regole ed eventualmente si sollevano gli errori.*/
					if(shark_rule2(pw, k, l, &k_child, &l_child) < 0) return -1;
					
					if((k > i || (k == i && l > j)) && k < pw->plan->nrow && l < pw->plan->ncol) 
						pw->plan->dtime[k][l] = -(pw->plan->dtime[k][l]) - 1;
						
					if(k_child >= 0 && ( k_child > i || (k_child == i && l_child > j)) && k_child < pw->plan->nrow && l_child < pw->plan->ncol ) 
						pw->plan->dtime[k_child][l_child] = -(pw->plan->dtime[k_child][l_child]) - 1;
				}
			}
		}
	}
	
	/*Viene ora percorsa la matrice in cerca dei pesci per poter applicare le funzioni fish_rule3 e fish_rule4 a ognuno di essi.*/
	for(i = 0; i < pw->plan->nrow; i++)
	{
		for(j = 0; j < pw->plan->ncol; j++)
		{
			k_child = l_child = -1;
			if(pw->plan->w[i][j] == FISH)
			{
				if(pw->plan->dtime[i][j] < 0)
					pw->plan->dtime[i][j] = -(pw->plan->dtime[i][j] + 1) ;
				else
				{
					if(fish_rule3(pw, i, j, &k, &l) < 0) return - 1;
					if(fish_rule4(pw, k, l, &k, &l) < 0) return -1;
					
					if(k > i || (k == i && l > j && k < pw->plan->nrow && l < pw->plan->ncol))
						pw->plan->dtime[k][l] = -(pw->plan->dtime[k][l]) - 1;
						
					if( k_child >= 0 && ( k_child > i || (k_child == i && l_child > j)) && k_child < pw->plan->nrow && l_child < pw->plan->ncol ) 
						pw->plan->dtime[k_child][l_child] = -(pw->plan->dtime[k_child][l_child]) - 1;
				}
			}
		}
	}
	
	/*Aggiorno i valori del pianeta.*/
	pw->nf = fish_count(pw->plan);
	pw->ns = shark_count(pw->plan);
	pw->chronon++;	
	
	return 0;
}

int safely_work(wator_t *pw, square_t *area, pthread_mutex_t **mutexes, int **check)
{
	int i, j, k, l, k_child, l_child;
	
	if(!pw || !area || ! mutexes) { errno = EINVAL; return -1; }
	
	for(j = area->left.y; j <= area->right.y; j++ )
	{	
		for(i = area->left.x; i <= area->right.x; i++)
		{
			#ifdef DEBUG
				printf("(x:%d, y:%d) -> (x:%d, y:%d)\n", area->left.x, area->left.y, area->right.x, area->right.y);
			#endif
			
			pthread_mutex_lock(&mutexes[UP(i, pw->plan->nrow)][j]);
			pthread_mutex_lock(&mutexes[i][LEFT(j, pw->plan->ncol)]);
			pthread_mutex_lock(&mutexes[i][j]);
			pthread_mutex_lock(&mutexes[i][RIGHT(j, pw->plan->ncol)]);
			pthread_mutex_lock(&mutexes[DOWN(i, pw->plan->nrow)][j]);			
			
			#ifdef DEBUG
				printf("STO LOCKANDO: (%d,%d):%c  (%d,%d):%c  (%d,%d):%c  (%d,%d):%c  (%d,%d):%c \n", 
						UP(i, pw->plan->nrow) , j, cell_to_char(pw->plan->w[UP(i, pw->plan->nrow)][j]),
						i , LEFT(j, pw->plan->ncol), cell_to_char(pw->plan->w[i][LEFT(j, pw->plan->ncol)]),
						i , j, cell_to_char(pw->plan->w[i][j]),
						i , RIGHT(j, pw->plan->ncol), cell_to_char(pw->plan->w[i][RIGHT(j, pw->plan->ncol)]),
						DOWN(i, pw->plan->nrow), j, cell_to_char(pw->plan->w[DOWN(i, pw->plan->nrow)][j]));
			#endif
			
			if(pw->plan->w[i][j] == FISH)
			{
				if(!check[i][j])
				{
					if(fish_rule4(pw, i, j, &k_child, &l_child) < 0){ return -1; }
					if(fish_rule3(pw, i, j, &k, &l) < 0) return - 1;
					
					if( k_child != i && l_child != j) 
						check[k_child][l_child] = 1;
					check[k][l] = 1;
				}
			}
			
			if(pw->plan->w[i][j] == SHARK)
			{
				if(!check[i][j])
				{
					int v ;
					if( (v=shark_rule2(pw, i, j, &k_child, &l_child)) < 0){printf("QUI!!!!!!!!!"); return -1; }
					if ( v != DEAD )
					{
						if(shark_rule1(pw, i, j, &k, &l) < 0) return -1;
						check[k][l] = 1;
					}
					
					if( k_child != i && l_child != j) 
						check[k_child][l_child] = 1;
				}
			}
			
			#ifdef DEBUG
				printf("STO RILASCIANDO: (%d,%d)\n", i, j);
			#endif
			
			pthread_mutex_unlock(&mutexes[DOWN(i, pw->plan->nrow)][j]);
			pthread_mutex_unlock(&mutexes[i][RIGHT(j, pw->plan->ncol)]);
			pthread_mutex_unlock(&mutexes[i][j]);
			pthread_mutex_unlock(&mutexes[i][LEFT(j, pw->plan->ncol)]);
			pthread_mutex_unlock(&mutexes[UP(i, pw->plan->nrow)][j]);
		}
	}
	
	return 0;
}

/********************************************************************************************************************/

/**
	Effettua il movimento dell'essere nella casella sopra.

	@param world puntatore alla struttura planet_t
	@param (x,y) coordinate dell'essere
	@param (k,l) nuove coordinate dell'essere
	@retval EAT se l'essere ha mangiato
	@retval MOVE se l'essere si è mosso
	@retval -1 in caso di errore
*/
int moveUP(planet_t *world, int x, int y, int *k, int *l)
{
	/*Si calcola il valore delle nuove coordinate.*/
	*k = UP(x, world->nrow);
	*l = HERE(y, world->ncol);
	
	/*Si gestisce il caso in cui il movimento rappresenti uno squalo in procinto di riprodursi.*/
	if(world->w[*k][*l] == FISH && world->w[x][y] == SHARK)
	{	
		world->w[*k][*l] = SHARK;
		world->w[x][y] = WATER;
		
		world->btime[x][y] = world->btime[*k][*l] = 0;
		world->dtime[x][y] = world->dtime[*k][*l] = 0;
		return EAT;
	}
	else if(world->w[*k][*l] == WATER) /*Si gestiscono tutti gli altri casi, che sono generalizzabili.*/
	{
		world->w[*k][*l] = world->w[x][y];
		world->w[x][y] = WATER;
		
		world->btime[*k][*l] = world->btime[x][y];
		world->btime[x][y] = 0;
		
		world->dtime[*k][*l] = world->dtime[x][y];
		world->dtime[x][y] = 0;	
		return MOVE;
	}
	else return -1;
}

/**
	Effettua il movimento dell'essere nella casella sotto.
	@param world puntatore alla struttura planet_t
	@param (x,y) coordinate dell'essere
	@param (k,l) nuove coordinate dell'essere
	@retval EAT se l'essere ha mangiato
	@retval MOVE se l'essere si è mosso
	@retval -1 in caso di errore
*/
int moveDOWN(planet_t *world, int x, int y, int *k, int *l)
{
	*k = DOWN(x, world->nrow);
	*l = HERE(y, world->ncol);

	if(world->w[*k][*l] == FISH && world->w[x][y] == SHARK)
	{
		world->w[*k][*l] = SHARK;
		world->w[x][y] = WATER;
		
		world->btime[x][y] = world->btime[*k][*l] = 0;
		world->dtime[x][y] = world->dtime[*k][*l] = 0;
		
		return EAT;
	}
	else if(world->w[*k][*l] == WATER)
	{
		world->w[*k][*l] = world->w[x][y];
		world->w[x][y] = WATER;
	 	return MOVE;

		world->btime[*k][*l] = world->btime[x][y];
		world->btime[x][y] = 0;
		
		world->dtime[*k][*l] = world->dtime[x][y];
		world->dtime[x][y] = 0;
	}
	else return -1;
}

/**
	Effettua il movimento dell'essere nella casella a sinistra.
	@param world puntatore alla struttura planet_t
	@param (x,y) coordinate dell'essere
	@param (k,l) nuove coordinate dell'essere
	@retval EAT se l'essere ha mangiato
	@retval MOVE se l'essere si è mosso
	@retval -1 in caso di errore
*/
int moveLEFT(planet_t *world, int x, int y, int *k, int *l)
{
	*k = HERE(x, world->nrow);
	*l = LEFT(y, world->ncol);
	
	if(world->w[*k][*l] == FISH && world->w[x][y] == SHARK)
	{
		world->w[*k][*l] = SHARK;
		world->w[x][y] = WATER;

		world->btime[x][y] = world->btime[*k][*l] = 0;
		world->dtime[x][y] = world->dtime[*k][*l] = 0;
		return EAT;
	}
	else if(world->w[*k][*l] == WATER)
	{
		world->w[*k][*l] = world->w[x][y];
		world->w[x][y] = WATER;

		world->btime[*k][*l] = world->btime[x][y];
		world->btime[x][y] = 0;
		
		world->dtime[*k][*l] = world->dtime[x][y];
		world->dtime[x][y] = 0;
		return MOVE;
	}
	else return -1;
}

/**
	Effettua il movimento dell'essere nella casella a destra.
	@param world puntatore alla struttura planet_t
	@param (x,y) coordinate dell'essere
	@param (k,l) nuove coordinate dell'essere
	@retval EAT se l'essere ha mangiato
	@retval MOVE se l'essere si è mosso
	@retval -1 in caso di errore
*/
int moveRIGHT(planet_t *world, int x, int y, int *k, int *l)
{
	*k = HERE(x, world->nrow);
	*l = RIGHT(y, (world->ncol));
	
	if(world->w[*k][*l] == FISH && world->w[x][y] == SHARK)
	{
		world->w[*k][*l] = SHARK;
		world->w[x][y] = WATER;

		world->btime[x][y] = world->btime[*k][*l] = 0;
		world->dtime[x][y] = world->dtime[*k][*l] = 0;
		return EAT;
	}
	else if(world->w[*k][*l] == WATER)
	{
		world->w[*k][*l] = world->w[x][y];
		world->w[x][y] = WATER;

		world->btime[*k][*l] = world->btime[x][y];
		world->btime[x][y] = 0;
		
		world->dtime[*k][*l] = world->dtime[x][y];
		world->dtime[x][y] = 0;
		return MOVE;
	}
	else return -1;
}

/**
	Crea un nuovo essere nella posizione (k,j) in base alle specifiche passate in input.
	
	@param *world puntatore alla struttura planet_t
	@param type tipo della nuova creatura
	@param (x,y) coordinate del padre
	@param (*k,*j) nuove coordinate
	@retval TRUE se tutto è andato bene
	@retval FALSE se si è verificato un errore
*/
BOOL createLife(planet_t *world, cell_t type, int x, int y, int direction, int *k, int *j)
{
	*k = x, *j = y; /*I nuovi valori coincidono inizialmente con i valori del padre.*/
	switch(direction) /*Effettuo il parsing della direzione e calcolo le nuove coordinate in base ad essa.*/
	{
		case 0:
			*k = UP(x, world->nrow);
			*j = HERE(y, world->ncol);
			break;
		case 1:
			*k = DOWN(x, world->nrow);
			*j = HERE(y, world->ncol);
			break;
		case 2:
			*k = HERE(x, world->nrow);
			*j = LEFT(y, world->ncol);
			break;
		case 3:
			*k = HERE(x, world->nrow);
			*j = RIGHT(y, world->ncol);
			break;
		default:
			return FALSE;
			break;
	}
	
	if(world->w[*k][*j] != WATER) /*Controllo che la posizione che occuperebbe il figlio non sia occupata.*/
		return FALSE;		
	
	world->w[*k][*j] = type; /*Cambio i valori della matrice.*/
	world->dtime[*k][*j] = 0;
	world->btime[*k][*j] = 0;
	return TRUE;
}

/**
	Dealloca memoria e reindirizza il puntatore.
	@param **mem indirizzo del puntatore che si vuole deallocare
*/
void free_chunk(void **mem)
{
	if(*mem) /*Verifico che il puntatore sia valido.*/
	{
		free(*mem); /*Libero memoria e faccio puntare a NULL.*/
		*mem = NULL;
	}
}

/**
	Inserisce nel buffer i riferimenti agli indici di movimento delle celle contenenti il valore type.
	@param type tipo da ricercare nelle celle vicine
	@param buffer array in cui memorizzare gli indici
	@param (x,y) coordinate della posizione di partenza
	@retval counter numero delle posizioni che soddisfano la ricerca
*/
int findIt(cell_t type, int *buffer, planet_t *pw, int x, int y)
{
	int counter = 0;
	
	if(pw->w[UP(x, pw->nrow)][HERE(y, pw->ncol)] == type)
	{
		buffer[counter] = 0;
		counter++;
	}
	
	if(pw->w[DOWN(x, pw->nrow)][HERE(y, pw->ncol)] == type)
	{
		buffer[counter] = 1;
		counter++;
	}
	
	if(pw->w[HERE(x, pw->nrow)][LEFT(y, pw->ncol)] == type)
	{
		buffer[counter] = 2;
		counter++;
	}
	
	if(pw->w[HERE(x, pw->nrow)][RIGHT(y, pw->ncol)] == type)
	{
		buffer[counter] = 3;
		counter++;
	}
	
	return counter;
}

/**
	Setta in maniera adeguata errno
	@param nerror nuovo valore di errno
	@return NULL
*/
void *error(int nerror)
{
	errno = nerror;
	return NULL;
}
