//muncher homework by Logan with help from Andrew, Kyler and the internet

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>

//global variables
char **buffer; // array storing my lines in
char **tempBuffer;
FILE *myFile;
int bufferSize;
int reading = 0; 
int measuring = 0;
int numbering = 0;
int printing = 0;
int eof = -1;

pthread_mutex_t mutexReader = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mutexMeasurer = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mutexNumberer = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mutexPrinter = PTHREAD_MUTEX_INITIALIZER; 

pthread_cond_t fullCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t emptyCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t printerCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t numbererCondition = PTHREAD_COND_INITIALIZER;

// definitions for prototypes
void *reader();
void *measurer();
void *numberer();
void *printer();
void inputError();
void fileError();
void threadError();

// This method takes input from a file and stores it into a global array for other methods to later use
void *reader(){
	char *line = NULL; // array getline will store the text in
	size_t length = 0; // the length of the line that i will later use 
	int totLines = 0; // variable to keep track of how many lines i have gone through whihc i will later use to stop my threads
		while(getline(&line,&length, myFile)>0){ // my loop goes as long as their is a next line, getline will return -1 or lwoer for error or EOF
			totLines++; // an increment 
			strtok(line, "\n"); // removing Enter charatcer at the end of the string
			pthread_mutex_lock(&mutexReader); // locking my thread doesnt go early
			while(reading >= bufferSize + printing){ // reader cannot go if if the bufferSize plus the printing counter is to low. Makes sure i stay within the buffer
					pthread_cond_signal(&printerCondition); // signaling my printer to check to see if it can go becasue reader is about to wait on it
					pthread_cond_wait(&fullCondition,&mutexReader); // waiting for the buffer to not be full
			}

			buffer[reading % bufferSize] = ( char* )malloc( length + 20 ); // allocating memory for the line size with extra for my other threads
			strcpy(buffer[reading % bufferSize], line); // copying my line into the buffer
			reading++; // increment so i know how many times i have gone through
			pthread_cond_signal(&emptyCondition); // singnal the buffer isnt empty anymore
			pthread_mutex_unlock(&mutexReader); // unlock my reader so others can go
		}
	eof = totLines; // after my while loop is done i exit and store the total number of lines so i know how many times each thread had to run
	return NULL; // we return here because once i reach the end of the file the reader thread 
}

// this Method takes a line from the global buffer and attaches the length of it in ()  at the end
void *measurer(){
	int length = 0; // counter to hold the length
	while(1){ // run for ever untill i return from the inside
		if(eof==measuring){ // making sure if they are equal i dont run again as this thread is now done
			pthread_cond_signal(&numbererCondition);  // signaling the numberer to go one last time
			return NULL; // returning back to main so it can rejoin
		}
		
		pthread_mutex_lock(&mutexMeasurer); // locking so i dont go prematurely 
		while(reading <= measuring){ // making sure i dont go before the reader has
			pthread_cond_signal(&fullCondition); // if measurer cant go signal the reader to try and go
		 	pthread_cond_wait(&emptyCondition, &mutexMeasurer);   // this is a fun line! make sure to wait the Measurer and not the Numberer
		} 

		length = strlen(buffer[measuring % bufferSize]); // grabbing the line out of the buffer and measuring
		sprintf(buffer[measuring % bufferSize], "%s (%d)", buffer[measuring % bufferSize], length); // appending () with its length
		measuring++; // counter so i know how many times i have ran
		pthread_cond_signal(&numbererCondition);  // telling the numberer it can go now
		pthread_mutex_unlock(&mutexMeasurer); // unlocking the buffer
		
			}

		return NULL;	// returning null if i somehow break the loop which it shouldnt
	}

// This method takes the buffer and adds the linenumber it is at the front of it with a colon and space
void *numberer(){
	int length = 0; // holder to know how long of an array i need to make
	while(1){ // run for ever untill i return from the inside
		if(eof == numbering){ // making sure if i need to go again or if i m done
			pthread_cond_signal(&printerCondition); // telling the printer check if it needs to run one last time
			return NULL;	// returning back to main so i can rejoin
		}
		pthread_mutex_lock(&mutexNumberer); // locking so I dont go prematurely 
		while(measuring <= numbering){ // making sure i am not going before the measurer does
				pthread_cond_signal(&emptyCondition); // if i cant go signal the one before to run 
		 		pthread_cond_wait(&numbererCondition, &mutexNumberer); //waiting for the signal to try and go
		}
		
		pthread_mutex_unlock(&mutexNumberer);// unlock so i can move on
		length = strlen(buffer[numbering % bufferSize]); // grabbing lenght so i know how big of an array i need
		char tempArray[length]; // making array
		strcpy(tempArray,buffer[numbering % bufferSize]);// storing the line into it real quick
		sprintf(buffer[numbering % bufferSize], "%d: %s", numbering+1, tempArray); // appending the number and : with a spcae and putting it back into the buffer
		numbering++; // incrementing so i know how many times i have run
		pthread_cond_signal(&printerCondition); // signaling the printer to run since i am done now		
	}
	return NULL; // just incase i ever somehow get out of the loop
}

//this method takes the buffer and prints it after the measurer and numberer have changed it
void *printer(){
	while(1){
			if(printing == eof){ // checking to see if i need to go again
		 		return NULL; // if i dont need to go again i return to the main so i canr ejoin
			}

		pthread_mutex_lock(&mutexPrinter); // locking so i dont go prematurely
		while(numbering <= printing ){ // checking to see that the printer never goes before the numberer

						pthread_cond_signal(&numbererCondition); // if i cant go tell the numberer to try and run to avoid being stuck
						pthread_cond_wait(&printerCondition, &mutexPrinter);// waiting till i get the signal befroe i try and run to go

			}
		printf("%s\n",buffer[printing % bufferSize]); // actually printing the line
		printing++; // keeping track of how many times i have gone
		pthread_cond_signal(&fullCondition);  // signalling that i am done and the printer can go again
		pthread_mutex_unlock(&mutexPrinter); // unlocking so i can move on

	}
}

//helper Error print method with finding and opening the file
void fileError(){
	printf("I cannot find that file! \n");
}

//helper error print method with taking input
void inputError(){
	printf("I need a two command line input containing a file followed by the buffer size!\n");
}

//helper error print method with threads
void threadError(){
	printf("Issue creating thread ");
}

int main(int argc, char **argv){ 
	if(argc != 3){ // makingsure i have three args
		inputError(); // if i dont throw error message
		exit(0); //exiting cause i cant handle the current input
	}

	if(myFile = fopen(argv[1], "r")){ // checking if i can find the file and open it
		//fclose(file);
	}else{
		fileError(); // throwing error message as i cant fidn the file
		exit(0); // exiting cause i cant run on nothing
	}
	char *bufferTemp; // temp array to grab argsv input from string form
	bufferTemp = argv[2]; // taaking the argument that should be the buffer and storing it.
	bufferSize = atoi(bufferTemp); //storing buffer size as an int

	buffer=(char**)malloc(bufferSize*sizeof(char*));
	pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t)*4);

	pthread_create(&threads[0],NULL,reader,NULL); // spinning of my threads 
	pthread_create(&threads[1],NULL,measurer,NULL);
	pthread_create(&threads[2],NULL,numberer,NULL);
	pthread_create(&threads[3],NULL,printer,NULL);
	for(int i=0;i<4;i++){ // rejoining my threads
		pthread_join(threads[i],NULL);
	}
	free(threads); // freeing memory
	free(buffer);

	return 0; // all done
}


