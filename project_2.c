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
pthread_mutex_t lastLock; //Lock to identify who will do the packaging
int lastAssembled, lastPainted, lastQAed = 0;

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

	//Thread launch
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&lastLock, NULL);

	//Initialize queues
	assemblyQue = ConstructQueue(1000);
	paintingQue = ConstructQueue(1000);
	packageQue = ConstructQueue(1000);
	deliveryQue = ConstructQueue(1000);
	QAQue = ConstructQueue(1000);

	//CONTROL BLOK
	pthread_t elfAThread;
	pthread_t elfBThread;
	pthread_t santaThread;
	pthread_t control_thread;

	pthread_create(&elfAThread, NULL, ElfA, NULL);
	pthread_create(&elfBThread, NULL, ElfB, NULL);
	pthread_create(&santaThread, NULL, Santa, NULL);
	pthread_create(&control_thread, NULL, ControlThread, NULL);

	pthread_sleep(simulationTime);

	pthread_cancel(elfAThread);	
	pthread_cancel(elfBThread);	
	pthread_cancel(santaThread);	
	pthread_cancel(control_thread); 	

	return 0;
}

void* ElfA(void *arg){
	while(true){
		Task package_task; //Packaging is prioritized, thus, lock if any packagingTask exits in the packageQue
		pthread_mutex_lock(&lock); 

		if(isEmpty(packageQue)){ //If empty lock must be released
			pthread_mutex_unlock(&lock);
		} 
		else {

			package_task = Dequeue(packageQue); //If task exist dequeue it 
			pthread_mutex_unlock(&lock); //Release lock

			pthread_sleep(1); //Do packaging job
			package_task.package_done = true; //Set packaging field to true of Task 
			printf("-------elfA PackagingDone = %d\n", package_task.ID);

			pthread_mutex_lock(&lock); //Lock
			Enqueue(deliveryQue, package_task); //Send the task to deliveryQue
			pthread_mutex_unlock(&lock); //Release lock

		} //Packaging end

		pthread_mutex_lock(&lock); //Lock
		if(!isEmpty(packageQue)){ // If package exists
			pthread_mutex_unlock(&lock); //Release lock after checking and
			continue; //skip iteration, look for packaging job again
		}
		pthread_mutex_unlock(&lock); // Else release lock and look for Painting jobs


		//Painting Task
		Task painting_task;
		pthread_mutex_lock(&lock); //lock
		if(isEmpty(paintingQue)){ //If the queue is empty
			pthread_mutex_unlock(&lock); //unlock 
		} else {
			painting_task = Dequeue(paintingQue); //else, dequeue it 
			pthread_mutex_unlock(&lock); //then release lock 

			pthread_sleep(3); //Do painting
			painting_task.painting_done = true; //Set painting true

			pthread_mutex_lock(&lastLock); //Lock
			lastPainted = painting_task.ID; //Update last painted int with id of the current task
			pthread_mutex_unlock(&lastLock); //Release 

			printf("-------elfA Painting done id = %d\n", painting_task.ID);

			if(painting_task.assembly_done && painting_task.QA_done && painting_task.painting_done) { //If no other jobs needed for the task
				pthread_mutex_lock(&lock); //Lock
				Enqueue(packageQue,painting_task); //Enqueue it to the packageQue
				pthread_mutex_unlock(&lock); //Release
			}
			else if (!painting_task.assembly_done || !painting_task.QA_done) { //If QA or assembly still needed
				pthread_mutex_lock(&lastLock); //Lock for lastModified 
				//Here this part is needed because of type 4 and type 5 gifts, since those types of gifts should be modified by both santa and either elfA or elfB but should be packaged by one of the elves. If elfA finished painting and waiting for santa(QA) to package. Since we now know what is the last task that is modified by elfA, elfB and Santa we can let the one that does his job latest do the packaging
				if(lastAssembled >= lastPainted && lastQAed >= lastPainted){ //Check if
					painting_task.assembly_done = true;
					painting_task.QA_done = true;
					pthread_mutex_lock(&lock);
					Enqueue(packageQue, painting_task);
					pthread_mutex_unlock(&lock);
				}	
				pthread_mutex_unlock(&lastLock);


			} else {
				printf("The task with id:%d need more job before package\n", painting_task.ID);
			}
		}
	}
}

void* ElfB(void *arg){
	while(true){	
		Task package_task; //Packaging is prioritized, thus, package if any packagingTask exits in the packageQue
		pthread_mutex_lock(&lock); 
		//critical section start

		if(isEmpty(packageQue)){ //If queue is empty
			pthread_mutex_unlock(&lock); //release lock
		} else {

			package_task = Dequeue(packageQue); // else, dequeue a job 
			pthread_mutex_unlock(&lock); //then release the lock
			//critical section end


			pthread_sleep(1); //Do packaging
			package_task.package_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(deliveryQue, package_task);
			pthread_mutex_unlock(&lock);

			printf("//++//++//elfB PackagingDone = %d\n", package_task.ID);

		} //Packaging Task 
	
		pthread_mutex_lock(&lock); //Lock
		if(!isEmpty(packageQue)){ // If package exists
			pthread_mutex_unlock(&lock); //Release lock after checking and
			continue; //skip iteration, look for packaging job again
		}
		pthread_mutex_unlock(&lock); // Else release lock and look for Assembly jobs

		//Assembly Task
		Task assembly_task;
		pthread_mutex_lock(&lock); //lock
		if(isEmpty(assemblyQue)){ //If empty 
			pthread_mutex_unlock(&lock); //then release lock
		} else {
			assembly_task = Dequeue(assemblyQue); //else dequeue a job
			pthread_mutex_unlock(&lock); //then release the lock

			pthread_sleep(2); //Do assembly job
			assembly_task.assembly_done = true;

			pthread_mutex_lock(&lastLock);
			lastAssembled = assembly_task.ID;
			pthread_mutex_unlock(&lastLock);

			printf("//++//++//elfB Assembly done id = %d\n", assembly_task.ID);

			if(assembly_task.QA_done && assembly_task.painting_done && assembly_task.assembly_done) { //IF all done
				pthread_mutex_lock(&lock); //lock
				Enqueue(packageQue,assembly_task); //send to package queue
				pthread_mutex_unlock(&lock); //release the lock
			} else if (!assembly_task.QA_done || !assembly_task.painting_done) {
				pthread_mutex_lock(&lastLock);
				if(lastPainted >= lastAssembled && lastQAed >= lastAssembled) {
					assembly_task.painting_done = true;
					assembly_task.QA_done = true;
					pthread_mutex_lock(&lock);
					Enqueue(packageQue, assembly_task);
					pthread_mutex_unlock(&lock);
				}
				pthread_mutex_unlock(&lastLock);

			} else {
				printf("The task with id:%d need more job before package\n", assembly_task.ID);
			}
		}
	}
}

// manages Santa's tasks
void* Santa(void *arg){
	while(true){	
		Task delivery_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(deliveryQue)){
			pthread_mutex_unlock(&lock);
		} else {
			delivery_task = Dequeue(deliveryQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(1);
			delivery_task.delivery_done = true;
			printf("HOHOHO!! Santa deliveryDone: %d\n",delivery_task.ID);
		} //Delivery job done, BUT need to check 2 conds and may be doing it again

		//This part is needed for Santa to do QA job first when one of the conditions hold
		pthread_mutex_lock(&lock);
		if(!isEmpty(deliveryQue) && QAQue->size < 3){ //If qa task is not > 3
			pthread_mutex_unlock(&lock); //And deliveryQue is not empty
			continue; //skip iteration, start new one
		}
		pthread_mutex_unlock(&lock); // Else release lock and look for QA jobs

		//QA Task part
		Task QA_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(QAQue)) {
			pthread_mutex_unlock(&lock);
		} else {
			pthread_mutex_unlock(&lock);
			while(QAQue->size >= 3){
				pthread_mutex_lock(&lock);
				QA_task = Dequeue(QAQue);
				pthread_mutex_unlock(&lock);


				pthread_sleep(1); //Do QA job
				QA_task.QA_done = 1;

				pthread_mutex_lock(&lastLock);
				lastQAed = QA_task.ID;
				pthread_mutex_unlock(&lastLock);

				printf("HOHOHOH!! Santa QADone: %d\n", QA_task.ID);


				if(QA_task.painting_done && QA_task.assembly_done && QA_task.QA_done) {
					pthread_mutex_lock(&lock);
					Enqueue(packageQue, QA_task);
					pthread_mutex_unlock(&lock);
				} else if (!QA_task.assembly_done || !QA_task.painting_done) {
					pthread_mutex_lock(&lastLock);
					if(lastAssembled >= lastQAed && lastPainted >= lastQAed){
						QA_task.assembly_done = true;
						QA_task.painting_done = true;
						pthread_mutex_lock(&lock);
						Enqueue(packageQue, QA_task);
						pthread_mutex_unlock(&lock);
					}
					pthread_mutex_unlock(&lastLock);
				}	
			}
		}
	}
}
// the function that controls queues and output
void* ControlThread(void *arg){

	int t_id = 1;
	while(true){
		pthread_sleep(1);

		Task task;
		task.ID = t_id;

		int rand_gift = rand() % 100; // [0,99]

		if(rand_gift >= 0 && rand_gift <= 39) { //chocolate [0,40)
			task.type = 1;
			task.assembly_done = true;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(packageQue, task);
			printf("\t\tNewGIFT-Chocolate with id: %d enqueued to package\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(40 <= rand_gift && rand_gift <=59){ //wooden toy + chocolate [40, 60)
			task.type = 2;
			task.assembly_done = true;
			task.painting_done = false;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(paintingQue, task);
			printf("\t\tNewGIFT-Wood_toy gift with id: %d enqueued to paint\n", t_id);	
			pthread_mutex_unlock(&lock);


		} else if(60 <= rand_gift && rand_gift <=79){ //plastic toy + chocolate [60, 80)
			task.type = 3;
			task.assembly_done = false;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(assemblyQue, task);
			printf("\t\tNewGIFT-Plastic_toy gift with id: %d enqueued to assembly\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(80 <= rand_gift && rand_gift <=84){ //GS + wooden toy + chocolate [80,85)
			task.type = 4;
			task.assembly_done = true;
			task.painting_done = false;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = false;

			pthread_mutex_lock(&lock);
			Enqueue(paintingQue, task);
			Enqueue(QAQue, task);
			printf("\t\tNewGIFT-GS + wood_toy gift with id: %d enqueued to QA & painting\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else if(85 <= rand_gift && rand_gift <=89){ //GS + plastic toy + chocolate [85, 90)
			task.type = 5;
			task.assembly_done = false;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = false;

			pthread_mutex_lock(&lock);
			Enqueue(assemblyQue, task);
			Enqueue(QAQue, task);
			printf("\t\tNewGIFT-GS + plastic_toy gift with id: %d enqueued to QA & assembly\n", t_id);	
			pthread_mutex_unlock(&lock);

		} else { // Again type 1 gift created [90, 100)
			task.type = 1;
			task.assembly_done = true;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(packageQue, task);
			printf("\t\tNewGIFT-Chocolate with id: %d enqueued to package\n", t_id);	
			pthread_mutex_unlock(&lock);
		}


		t_id++;
	}

}
