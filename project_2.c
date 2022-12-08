#include "queue.c"

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 30; // frequency of emergency gift requests from New Zealand

void* ElfA(void *arg); // the one that can paint
void* ElfB(void *arg); // the one that can assemble
void* Santa(void *arg); 
void* ControlThread(void *arg); // handles printing and queues (up to you)

	pthread sleeper function
int pthread_sleep (int seconds)
{
	pthread_mutex_t mutex;
	pthread_cond_t conditionvar;
	struct timespec timetoexpire;
	if(pthread_mutex_init(&mutex,NULL))
	{
		return -1;
	}
	if(pthread_cond_init(&conditionvar,NULL))
	{
		return -1;
	}
	struct timeval tp;
	//When to expire is an absolute time, so get the current time and add it to our delay time
	gettimeofday(&tp, NULL);
	timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;

	pthread_mutex_lock(&mutex);
	int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&conditionvar);

	//Upon successful completion, a value of zero shall be returned
	return res;
}


int main(int argc,char **argv){
	// -t (int) => simulation time in seconds
	// -s (int) => change the random seed
	for(int i=1; i<argc; i++){
		if if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
		else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
	}

	srand(seed); // feed the seed

	/* Queue usage example
	   Queue *myQ = ConstructQueue(1000);
	   Task t;
	   t.ID = myID;
	   t.type = 2;
	   Enqueue(myQ, t);
	   Task ret = Dequeue(myQ);
	   DestructQueue(myQ);
	   */

	// your code goes here
	// you can simulate gift request creation in here, 
	// but make sure to launch the threads first

	return 0;
}

void* ElfA(void *arg){

}

void* ElfB(void *arg){

}

// manages Santa's tasks
void* Santa(void *arg){

}

// the function that controls queues and output
void* ControlThread(void *arg){

}
