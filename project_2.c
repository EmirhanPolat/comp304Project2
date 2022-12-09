#include "queue.c"
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#ifdef __APPLE__
	#include <dispatch/dispatch.h>
	typedef dispatch_semaphore_t psem_t;
#else
	#include <semaphore.h> 
	typedef sem_t psem_t;
#endif

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 30; // frequency of emergency gift requests from New Zealand

void* ElfA(void *arg); // the one that can paint
void* ElfB(void *arg); // the one that can assemble
void* Santa(void *arg); 
void* ControlThread(void *arg); // handles printing and queues (up to you)

//We will need queues for every operation
Queue *assemblyQue;
Queue *paintingQue;
Queue *packageQue;
Queue *QAQue;
Queue *deliveryQue;

pthread_mutex_t lock; //Basic lock for all the queues 
pthread_mutex_t lock_assembly, lock_painting, lock_package, lock_QA, lock_delivery; //every department has their own lock 

//pthread sleeper function
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
		printf("%s and %s\n",argv[1], argv[2]);
		if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
		else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
	}

	srand(seed); // feed the seed

	/*
	//Queue usage example
	int myID = 3;
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
	printf("Hello\n");

	//Initialize queues
	assemblyQue = ConstructQueue(1000);
	paintingQue = ConstructQueue(1000);
	packageQue = ConstructQueue(1000);
	deliveryQue = ConstructQueue(1000);
	QAQue = ConstructQueue(1000);

	//CONTROL BLOK

	pthread_mutex_init(&lock, NULL);

	pthread_t elfAThread;
	pthread_t elfBThread;
	pthread_t santaThread;
	pthread_t control_thread;
	int sim = 0;
	while(sim < simulationTime) {
		pthread_create(&elfAThread, NULL, ElfA, NULL);
		pthread_create(&elfBThread, NULL, ElfB, NULL);
		pthread_create(&santaThread, NULL, Santa, NULL);
		pthread_create(&control_thread, NULL, ControlThread, NULL);

		pthread_join(elfAThread, NULL);	
		pthread_join(elfBThread, NULL);	
		pthread_join(santaThread, NULL);	
		pthread_join(control_thread, NULL); 
	
			sim++;
	}
	return 0;
}

void* ElfA(void *arg){
	while(1){
		Task package_task; //Packaging is prioritized, thus, package if any packagingTask exits in the packageQue
		pthread_mutex_lock(&lock); 
		//critical section start

		if(isEmpty(packageQue)){
			pthread_mutex_unlock(&lock);
			//critical section end
		} else {

			package_task = Dequeue(packageQue);
			pthread_mutex_unlock(&lock);
			//critical section end

			pthread_sleep(1);
			package_task.package_done = true;
			printf("-----elfA Packaging done id = %d\n", package_task.ID);

			pthread_mutex_lock(&lock);
			Enqueue(deliveryQue, package_task);
			pthread_mutex_unlock(&lock);

		} //Packaging end 

		//Painting Task
		Task painting_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(paintingQue)){
			pthread_mutex_unlock(&lock);
		} else {
			painting_task = Dequeue(paintingQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(3);
			painting_task.painting_done = true;
			printf("elfA Painting done id = %d\n", painting_task.ID);

			if(painting_task.assembly_done && painting_task.QA_done && painting_task.painting_done) {
				pthread_mutex_lock(&lock);
				Enqueue(packageQue,painting_task);
				pthread_mutex_unlock(&lock);

			} else {
				printf("The task with id:%d need more job before package\n", painting_task.ID);
			}
		}



	}
}

void* ElfB(void *arg){
	while(1){
		Task package_task; //Packaging is prioritized, thus, package if any packagingTask exits in the packageQue
		pthread_mutex_lock(&lock); 
		//critical section start

		if(isEmpty(packageQue)){
			pthread_mutex_unlock(&lock);
			//critical section end
		} else {

			package_task = Dequeue(packageQue);
			pthread_mutex_unlock(&lock);
			//critical section end

			pthread_sleep(1);
			package_task.package_done = true;
			printf("-----elfB Packaging done id = %d\n", package_task.ID);

			pthread_mutex_lock(&lock);
			Enqueue(deliveryQue, package_task);
			pthread_mutex_unlock(&lock);

		} //Packaging end 

		//Assembly Task
		Task assembly_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(assemblyQue)){
			pthread_mutex_unlock(&lock);
		} else {
			assembly_task = Dequeue(assemblyQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(2);
			assembly_task.assembly_done = true;
			printf("elfB Assembly done id = %d\n", assembly_task.ID);

			if(assembly_task.QA_done && assembly_task.painting_done && assembly_task.assembly_done) {
				pthread_mutex_lock(&lock);
				Enqueue(packageQue,assembly_task);
				pthread_mutex_unlock(&lock);

			} else {
				printf("The task with id:%d need more job before package\n", assembly_task.ID);
			}
		}



	}

}

// manages Santa's tasks
void* Santa(void *arg){
	while(1) {
		Task delivery_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(deliveryQue)){
			pthread_mutex_unlock(&lock);
		} else {
			delivery_task = Dequeue(deliveryQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(1);
			delivery_task.delivery_done = true;
			printf("HOHOHO!! Santa delivered task with id: %d\n",delivery_task.ID);
		}
		Task QA_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(QAQue)) {
			pthread_mutex_unlock(&lock);
		} else {
			QA_task = Dequeue(QAQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(1);
			QA_task.QA_done = 1;
			printf("Santa finished QA for task %d\n", QA_task.ID);

			if(QA_task.painting_done && QA_task.assembly_done && QA_task.QA_done) {
				pthread_mutex_lock(&lock);
				Enqueue(packageQue, QA_task);
				pthread_mutex_unlock(&lock);
			}
		}
	}	
}

// the function that controls queues and output
void* ControlThread(void *arg){
	int t_id = 1;
	while(1) {
		pthread_sleep(1);

		Task task;
		task.ID = t_id;

		int rand_gift = rand() % 100; // [0,99]

		if(rand_gift >= 0 && rand_gift <= 39) { //chocolate
			task.type = 1;
			task.assembly_done = true;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(packageQue, task);
			printf("\tNewGIFT-Chocolate with id: %d enqueued to package\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(40 <= rand_gift && rand_gift <=59){ //wooden toy + chocolate
			task.type = 2;
			task.assembly_done = true;
			task.painting_done = false;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(paintingQue, task);
			printf("\tNewGIFT-Wood_toy gift with id: %d enqueued to paint\n", t_id);	
			pthread_mutex_unlock(&lock);


		} else if(60 <= rand_gift && rand_gift <=79){ //plastic toy + chocolate
			task.type = 3;
			task.assembly_done = false;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(assemblyQue, task);
			printf("\tNewGIFT-Plastic_toy gift with id: %d enqueued to assembly\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(80 <= rand_gift && rand_gift <=84){ //GS + wooden toy + chocolate
			task.type = 4;
			task.assembly_done = true;
			task.painting_done = false;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = false;

			pthread_mutex_lock(&lock);
			Enqueue(paintingQue, task);
			Enqueue(QAQue, task);
			printf("\tNewGIFT-GS + wood_toy gift with id: %d enqueued to QA & painting\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(85 <= rand_gift && rand_gift <=89){ //GS + plastic toy + chocolate
			task.type = 5;
			task.assembly_done = false;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = false;

			pthread_mutex_lock(&lock);
			Enqueue(assemblyQue, task);
			Enqueue(QAQue, task);
			printf("\tNewGIFT-GS + plastic_toy gift with id: %d enqueued to QA & assembly\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else {
			printf("\tNewGIFT-No gift this time\n");
		}


		t_id++;

	}

}
