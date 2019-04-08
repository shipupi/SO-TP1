#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <semaphore.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <sys/wait.h>
#include <time.h>
#include "hashfiles.h"




int waitForPid();
int main(int argc,char * argv[]){

	// Check hashfile Process exist
	int hashfilesPID = waitForPid();
 
    // Init variables
    struct timespec tm;
    int semval;
	int shmid;
	key_t key = ftok(SHMKEYSTR,SHMKEYNUM); 
	char  * shm;
	char * s;
	int offset = 0;
	char c;
	int i;

	// Init semaphores
	sem_t *hashReadySemaphore = sem_open(HASHREADYSEM, O_CREAT, 0644, 0);
	sem_t *shmReadySemaphore = sem_open(SHMREADYSEM, O_CREAT, 0644, 0);

	sem_wait(shmReadySemaphore);
	shmid = shmget(key,SHSIZE,0666);

	if(shmid < 0){
		perror("shmget");
		exit(1);
	}

	shm = shmat(shmid,NULL,0);
	if(shm== (char *)-1){
		perror("shmat");
		exit(1);
	}
	s = shm;

	while(kill(hashfilesPID, 0) >= 0) { // Hashfiles app is open, let's wait for the semaphore
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += 1;
		sem_timedwait(hashReadySemaphore, &tm); // Max timeout of 5 seconds, in case app is already dead to avoid deadlock
		while((c = *(s + offset)) != 0) {
			printf("%c",c);		
			offset++;
		}
		memset(s, 0, BUFFERSIZE); // Clean the shared memory after using it ( In case it may be reused when SH is full)
		s += BUFFERSIZE; // Pass to the next block
		if ((s - shm) == SHSIZE) { // Reset to the first Block if all SHM is used
			s = shm;
		}
		offset = 0;
	}


	// Hashfiles app is closed
	// Printing remaining files 
	sem_getvalue(hashReadySemaphore, &semval);
	for (i = 0; i < semval; ++i)
	{
		while((c = *(s + offset)) != 0) {
			printf("%c",c);		
			offset++;
		}
		memset(s, 0, BUFFERSIZE);
		s += BUFFERSIZE;
		if ((s - shm) == SHSIZE) {
			s = shm;
		}
		offset = 0;	
	}

	// cleanup
	shmdt(shm);
	shmctl(shmid,IPC_RMID,NULL); 
	sem_close(hashReadySemaphore);
	sem_unlink(HASHREADYSEM);
	sem_close(shmReadySemaphore);
	sem_unlink(SHMREADYSEM);
	return 0;
}

// Parte de este bloque sacado de : https://stackoverflow.com/questions/51913506/how-to-add-a-timeout-when-reading-from-stdin
int waitForPid() {
	int LEN = 20;
	char cpid[LEN];
	struct timeval timeout = {10, 0}; // Timeout of 10 seconds to receive a pid
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ret = select(1, &fds, NULL, NULL, &timeout);
    if (ret == -1) {
        printf("Oops! Something wrong happened...\n");
        exit(0);
    } else if (ret == 0) {
        printf("No pid was received, exiting.\n");
        exit(0);
    } else {
        fgets(cpid, LEN, stdin);
    }
    int hashfilesPID = atoi(cpid); 
    if (hashfilesPID <= 0)
    {
    	printf("Invalid pid input. Exiting\n");
    	exit(0);
    }

    if (kill(hashfilesPID, 0) < 0)
    {
    	printf("Hashfiles process doesn't exist. Exiting\n");
    	exit(0);
    }
    return hashfilesPID;
}


