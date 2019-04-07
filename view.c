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


#define SHSIZE 30720
#define BUFFERSIZE 512

int main(int argc,char * argv[]){

	// Bloque sacado de : https://stackoverflow.com/questions/51913506/how-to-add-a-timeout-when-reading-from-stdin
	int LEN = 20;
	char cpid[LEN];
	struct timeval timeout = {3, 0};
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
    // End
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


	int shmid = -1;
	key_t key = ftok("./hashfiles.fl",1337); 
	char  * shm;
	char * s;
	int printing = 0;
	int offset = 0;


	while(shmid == -1) {
		shmid = shmget(key,SHSIZE,0666);
	}

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
	while(1) {
		char c;
		c = *(s + offset);
		if (c != 0){
			printing = 1;
			printf("%c",c);		
			offset++;
		} else {
			if (printing == 1) {
				printing = 0;
				memset(s, 0, BUFFERSIZE);
				s += BUFFERSIZE;
				if ((s - shm) == SHSIZE) {
					s = shm;
				}
				offset = 0;
			} else if (kill(hashfilesPID, 0) < 0) {// Received 0 and parent process is closed. No more info is coming
				break;	
			}
		}
	}
	shmdt(shm);
	shmctl(shmid,IPC_RMID,NULL); 
	return 0;
}
