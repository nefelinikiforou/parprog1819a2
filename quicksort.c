#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
//gcc -pthread -O2 quicksort.c -o qsort

/* Constants */
#define THREADS 4		//Constant number of threads
#define LIMIT 250000		//Treshold to define whether to use threading or not, to avoid the creation of numerous threads
#define CUTOFF 10		//Treshold to define which sorting we will use
#define N 1000000		//Size of queue
#define SIZE 100		//Size of unsorted matrix 

/* Global variables */
int queue_size = N;					//size of queue
pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER;	//conditional variable msg_in
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER;	//conditional variable msg_out
int qin = 0;						//position of message inserted in queue
int qout = 0;						//position of message extracted from queue
int message_count = 0;					//number of messages in queue
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Stucts */
struct message {
	int type;	//type of message: 0 == WORK, 1 == FINISH, 2 == SHUTDOWN
	int first;	//position of the first element of the matrix fragment in message
	int last;	//position of the last element of the matrix fragment in message
};

//struct thread_params {		//for every thread
//	double *a;
//	int n;
//};

struct message msg_queue[N];

/* Function for sending message */
void send (int type, int first, int last) {
	pthread_mutex_lock(&mutex);

	while(message_count >= queue_size) {
			pthread_cond_wait(&msg_out, &mutex);	//mutex is locked the next time producer needs it
	}

	msg_queue[qin].type = type;	//Copy content
	msg_queue[qin].first = first;	//Copy content
	msg_queue[qin].last = last;	//Copy content
	qin = (qin + 1) % N;
	message_count += 1;

	if (qin >= queue_size) {	//Return to start of queue
		qin = 0;
	}

	pthread_cond_signal(&msg_in);
	pthread_mutex_unlock(&mutex);
}

/* Function for receiving message */
void recv (int * type, int * first, int * last) {		//Copy address
	pthread_mutex_lock(&mutex);

	while(message_count < 1) {
			pthread_cond_wait(&msg_in, &mutex);	//mutex is locked the next time producer needs it
	}

	*type = msg_queue[qout].type;
	*first = msg_queue[qout].first;
	*last = msg_queue[qout].last;
	qout = (qout + 1) % N;
	message_count -= 1;

	if (qout >= queue_size) {	//Return to start of queue
		qout = 0;
	}

	pthread_cond_signal(&msg_out);
	pthread_mutex_unlock(&mutex);
}
/* Function for insertion sort */
void inssort(double *a, int n) {
	double t;
	int j;
	for (int i=1; i<n; i++) {
		j=i;
		while ((j>0) && (a[j-1] > a[j])) {
			t = a[j-1];
			a[j-1] = a[j];
			a[j] = t;
			j--;
		}
	}
}

/* Function for partition (helper function for quicksort) */
int partition (double *a, int n) {
	int first = 0;
	int middle = n/2;
	int last = n-1;
	double temp;

	if (a[first] > a[middle]) {
		temp = a[middle];
		a[middle] = a[first];
		a[first] = temp;
	}
	if (a[middle] > a[last]) {
		temp = a[middle];
		a[middle] = a[last];
		a[last] = temp;
	}
	if (a[first] > a[middle]) {
		temp = a[middle];
		a[middle] = a[first];
		a[first] = temp;
	}

	int i,j;
	double pivot = a[middle];
	for (i=1, j=n-2 ;; i++, j--) {

		//find element that needs sorting	
		while(a[i]<pivot) {
			i++;
		}
		while(a[j]>pivot) {
			j--;
		}
		if (i >=j) {
			break;
		}

		temp = a[i];
		a[i] = a[j];
		a[j] = temp;

	}
	return i;
}

/* Function for quicksort */
void quicksort(double *a, int n, int t) {
	if (n<=CUTOFF) {	//if the size is smaller than or equal to treshold, use insertion sort
		inssort(a, n);
		return;
	}

	int i;


	if (t !=2 ) {		//If the type of message is not SHUTDOWN

		i = partition(a,n);
	//First half
		quicksort(a, i, t);
		send(0,*a,*a+i);	//begin sorting of first half
	//Second half
		quicksort(a+i, n-i, t);
		send(0,*a+i,n);		//begin sorting of second half
	}
}

/* Function thread_func */
void * thread_func(void * params){
	int t, f, l;
	
	double * a = (double *) params;

	for(;;) {
		recv(&t, &f, &l);
		if (t == 0) {		//WORK
			printf("WORK\n");
			quicksort(a+f,l-f,t);
			send(1,f,l);
			printf("FINISH\n");
		}
		else if (t == 2) {	//SHUTDOWN
			send(2,f,l);			
			break;
		}
		else {				//FINISH
			send(t,f,l);
		}
	}
	pthread_exit(NULL);	//exit
}

int main () {
	double * a = (double*) malloc(SIZE *  sizeof(double));

	if (a == NULL) {
		printf("Allocation error\n");
		exit(1);
	}

	srand(time(NULL));

	//Initialization
	for (int i=0; i<SIZE; i++) {
		a[i] = (double) rand()/RAND_MAX;
	}

	//Thread pool creation
	pthread_t mythread[THREADS];

	for (int i=0; i<THREADS; i++) {
		if (pthread_create(&mythread[i], NULL, thread_func, a) != 0) {
			printf("Thread pool creation error\n");
			free(a);
			exit(1);
		}
	}

	//Start sorting
	send(0,0,SIZE);		//WORK, Position of first element of matrix a, position of last element of matrix a
	
	int t, f, l;
	int completed = 0;
	while (completed <= SIZE) {
		recv(&t,&f,&l);
	
		if (t == 1) {	//FINISH
			completed += 1;
		}
		else {				//WORK
			send(t,f,l);
		}
	}

	send(2,0,0);	//send SHUTDOWN
	printf("SEND SHUTDOWN\n");

	for (int i=0; i<THREADS; i++) {
		pthread_join(mythread[i], NULL);		//join main thread
	}

	//Check
	for (int i=0; i<(SIZE-1); i++) {
		if (a[i] > a[i+1]) {
			printf("%lf, %lf\n", a[i], a[i+1]);
			printf("error\n");
			break;
		}
	}



	//Deallocation
	free(a);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&msg_in);
	pthread_cond_destroy(&msg_out);
	return 0;
}
