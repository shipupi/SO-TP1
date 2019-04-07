#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHSIZE 100

int main(int argc,char * argv[]){
	int shmid = -1;
	key_t key = ftok("./hashfiles.fl",1337); 
	char  * shm;
	char * s;
	char c;
	int printing = 0;
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
		c = *s;
		if (c != 0){
			printing = 1;
			printf("%c",c);		
			s++;
		} else {
			if (printing == 1) {
				printing = 0;
				printf("\n");
				s++;
			}
		}
	}
	shmdt(shm);
	shmctl(shmid,IPC_RMID,NULL); 
	return 0;
}
