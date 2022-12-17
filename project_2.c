#include "queue.c"
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

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
pthread_mutex_t timeLock; //Lock when updating time 
pthread_mutex_t taskCountLock; //Lock when modifying global variable taskCount
int lastAssembled, lastPainted, lastQAed = 0; // We dont need lastPackaged and lastDelivered 
int current_time, task_count = 0; //Current time and current task count

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
	pthread_mutex_init(&timeLock, NULL);
	pthread_mutex_init(&taskCountLock, NULL);

	//Initialize queues
	assemblyQue = ConstructQueue(1000);
	paintingQue = ConstructQueue(1000);
	packageQue = ConstructQueue(1000);
	deliveryQue = ConstructQueue(1000);
	QAQue = ConstructQueue(1000);
	
	printf("TaskID  GiftID  GiftType  TaskType  RequestTime  TaskArrival  TT  Responsible\n");
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
			//printf("Elf-A -----> PackagingDone = %d\n", package_task.ID);
			
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);
			
			pthread_mutex_lock(&taskCountLock);
			printf("%-8d%-8d%-10dC\t\t%-9d%-13d%-4dA\n",++task_count, package_task.ID, package_task.type, package_task.start, end_time, end_time-package_task.start);
			pthread_mutex_unlock(&taskCountLock);

			pthread_mutex_lock(&lock); //Lock
			Enqueue(deliveryQue, package_task); //Send the task to deliveryQue
			
			if(!isEmpty(packageQue)){
				pthread_mutex_unlock(&lock);
				continue;
			}
			pthread_mutex_unlock(&lock);
		} //Packaging End
		
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
			//printf("Elf-A -----> Painting done id = %d\n", painting_task.ID);
	
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);
			
			pthread_mutex_lock(&taskCountLock);

			printf("%-8d%-8d%-10dP\t\t%-9d%-13d%-4dA\n",++task_count, painting_task.ID, painting_task.type, painting_task.start, end_time, end_time-painting_task.start);
			pthread_mutex_unlock(&taskCountLock);

			pthread_mutex_lock(&lastLock); //Lock
			lastPainted = painting_task.ID; //Update last painted int with id of the current task
			pthread_mutex_unlock(&lastLock); //Release 


			if(painting_task.assembly_done && painting_task.QA_done && painting_task.painting_done) { //If no other jobs needed for the task
				pthread_mutex_lock(&lock); //Lock
				Enqueue(packageQue,painting_task); //Enqueue it to the packageQue
				pthread_mutex_unlock(&lock); //Release
			}
			else if (!painting_task.QA_done) { //If QA is still needed
				//Here this part is needed because of type 4 and type 5 gifts, since those types of gifts should be modified by both santa and either elfA or elfB but 	should be packaged by one of the elves. If elfA finished painting and waiting for santa(QA) to package. By keeping the last modified item by every operator we know the last task that is modified by elfA, elfB and Santa. We first check if qa is done on the current task or not and if no, look if the last QAed item is greater or equal (meaning that its QA is done by santa) than current gift, if yes, this means that QA task actually done. So our elfA can change the qaDone flag of the task and send it to packageQue

				pthread_mutex_lock(&lastLock); //Lock for lastModified 
				if(lastQAed >= lastPainted){ //Check if
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
			//printf("Elf-B -----> PackagingDone = %d\n", package_task.ID);
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);	

			pthread_mutex_lock(&taskCountLock);
			printf("%-8d%-8d%-10dC\t\t%-9d%-13d%-4dB\n",++task_count, package_task.ID, package_task.type, package_task.start, end_time, end_time-package_task.start);
			pthread_mutex_unlock(&taskCountLock);

			pthread_mutex_lock(&lock);
			Enqueue(deliveryQue, package_task);
			
			if(!isEmpty(packageQue)){
				pthread_mutex_unlock(&lock);
				continue;
			}
			pthread_mutex_unlock(&lock);

		} //Packaging End 
			
		//Painting Task
		Task assembly_task;
		pthread_mutex_lock(&lock); //lock
		if(isEmpty(assemblyQue)){ //If the queue is empty
			pthread_mutex_unlock(&lock); //unlock 

		} else {
			assembly_task = Dequeue(assemblyQue); //else dequeue a job
			pthread_mutex_unlock(&lock); //then release the lock

			pthread_sleep(2); //Do assembly job
			assembly_task.assembly_done = true;
			//printf("Elf-B -----> Assembly done id = %d\n", assembly_task.ID);
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);

			pthread_mutex_lock(&taskCountLock);
			printf("%-8d%-8d%-10dA\t\t%-9d%-13d%-4dB\n",++task_count, assembly_task.ID, assembly_task.type, assembly_task.start, end_time, end_time-assembly_task.start);
			
			pthread_mutex_unlock(&taskCountLock);

			pthread_mutex_lock(&lastLock);
			lastAssembled = assembly_task.ID;
			pthread_mutex_unlock(&lastLock);

			if(assembly_task.QA_done && assembly_task.painting_done && assembly_task.assembly_done) { //IF all done
				pthread_mutex_lock(&lock); //lock
				Enqueue(packageQue,assembly_task); //send to package queue
				pthread_mutex_unlock(&lock); //release the lock
			} else if (!assembly_task.QA_done) {
				pthread_mutex_lock(&lastLock);
				if(lastQAed >= lastAssembled) {
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
			//printf("HOHOHO!! Santa deliveryDone: %d\n",delivery_task.ID);
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);
		
			pthread_mutex_lock(&taskCountLock);	
			printf("%-8d%-8d%-10dD\t\t%-9d%-13d%-4dS\n",++task_count, delivery_task.ID, delivery_task.type, delivery_task.start, end_time, end_time-delivery_task.start);
			pthread_mutex_unlock(&taskCountLock);
			
			pthread_mutex_lock(&lock);	
			if(QAQue->size < 3 || !isEmpty(deliveryQue)){
				pthread_mutex_unlock(&lock);
				continue;	
			}
			pthread_mutex_unlock(&lock);


		} //Delivery job done, BUT need to check 2 conds and may be doing it again

		//This part is needed for Santa to do QA job first when one of the conditions hold
		//QA Task part
		Task QA_task;
		pthread_mutex_lock(&lock);
		if(isEmpty(QAQue)) {
			pthread_mutex_unlock(&lock);
		} else {
			QA_task = Dequeue(QAQue);
			pthread_mutex_unlock(&lock);

			pthread_sleep(1); //Do QA job
			QA_task.QA_done = 1;
			//printf("Santa -----> QADone: %d\n", QA_task.ID);
			pthread_mutex_lock(&timeLock);
			int end_time = current_time;
			pthread_mutex_unlock(&timeLock);

			pthread_mutex_lock(&taskCountLock);
			printf("%-8d%-8d%-10dQ\t\t%-9d%-13d%-4dS\n",++task_count, QA_task.ID, QA_task.type, QA_task.start, end_time, end_time-QA_task.start);
			pthread_mutex_unlock(&taskCountLock);

			pthread_mutex_lock(&lastLock);
			lastQAed = QA_task.ID;
			pthread_mutex_unlock(&lastLock);

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
// the function that controls queues and output
void* ControlThread(void *arg){

	int t_id = 1;
	while(true){
		pthread_sleep(1);
		int rand_gift = rand() % 100; // [0,99]

		pthread_mutex_lock(&timeLock);
		current_time++;
		pthread_mutex_unlock(&timeLock);

		Task task;
		task.ID = t_id;
		task.start = current_time;

		if(rand_gift >= 0 && rand_gift <= 39) { //chocolate [0,40)
			task.type = 1;
			task.assembly_done = true;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(packageQue, task);
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type1 id: %d\n", t_id);	

		} else if(40 <= rand_gift && rand_gift <=59){ //wooden toy + chocolate [40, 60)
			task.type = 2;
			task.assembly_done = true;
			task.painting_done = false;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(paintingQue, task);
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type2 id: %d\n", t_id);	


		} else if(60 <= rand_gift && rand_gift <=79){ //plastic toy + chocolate [60, 80)
			task.type = 3;
			task.assembly_done = false;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(assemblyQue, task);
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type3 id: %d\n", t_id);	

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
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type4 id: %d\n", t_id);	

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
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type5 id: %d\n", t_id);	

		} else { // Again type 1 gift created [90, 100)
			task.type = 1;
			task.assembly_done = true;
			task.painting_done = true;	
			task.package_done = false;
			task.delivery_done = false;
			task.QA_done = true;

			pthread_mutex_lock(&lock);
			Enqueue(packageQue, task);
			pthread_mutex_unlock(&lock);
		//	printf("\t\tNewGIFT-Type1 id: %d\n", t_id);	
		}


		t_id++;
	}

}
