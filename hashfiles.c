#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <signal.h>
#include <sys/shm.h>          /* For share memory*/
#include "hashfiles.h"



void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]);
void killChild(pid_t pid);
void killChilds(pid_t pids[], int numberOfSlaves);
char *  initSharedMemory(sem_t * shmReadySemaphore, int *shmid);
int sendFilesToSlaves(int totalFiles, int numberOfSlaves, int filesPerSlave, char *argv[], int filePipe[]);
void sendHashesToOutputs(int totalFiles, int hashPipe[], char * shm , sem_t *hashReadySemaphore);
void setSemaphoreValue(sem_t *sem, int value);



int main (int argc, char *argv[]){
  
  // Init semaphores (Also empty semahphores to avoid previous crashes conflicts?)
  sem_t *hashReadySemaphore = sem_open(HASHREADYSEM, O_CREAT, 0644, 0);
  setSemaphoreValue(hashReadySemaphore, 0);
  sem_t *shmReadySemaphore = sem_open(SHMREADYSEM, O_CREAT, 0644, 0);
  setSemaphoreValue(shmReadySemaphore, 0);

  // Init View shared variables (and clean semaphores)
  printf("%d\n", getpid());  
  fflush(stdout); // Must flush instantly to let view process know the PID


  sleep(1); // wait for view

  // Init variables
  int numberOfSlaves = 2;
  int filesPerSlave = 1;
  pid_t slaveIds[numberOfSlaves];
  int totalFiles = argc - 1;
  int *shmid = malloc(8);  

  char * shm = initSharedMemory(shmReadySemaphore, shmid);


  // Init pipes
  int hashPipe[2], filePipe[2];
  pipe(hashPipe);
  pipe(filePipe);

  // Semaphore init
  // Closing the semaphore to avoid previous non-closed semaphores conflicts
  sem_unlink(FILEPIPEREADYSEM);
  sem_t *filePipeReadySemaphore = sem_open(FILEPIPEREADYSEM, O_CREAT, 0644, 1);
  
  // Create slaves
  createSlaves(argv[0], numberOfSlaves, hashPipe, filePipe, slaveIds);

  // Send Files to Slaves
  totalFiles = sendFilesToSlaves(totalFiles, numberOfSlaves, filesPerSlave, argv, filePipe);

  // Send Hashes to Outputs
  sendHashesToOutputs(totalFiles, hashPipe, shm, hashReadySemaphore);

  // Cleanup
  killChilds(slaveIds, numberOfSlaves);
  close(hashPipe[0]);
  close(filePipe[1]);
  sem_close(filePipeReadySemaphore);
  sem_unlink(FILEPIPEREADYSEM);
  sem_close(shmReadySemaphore);
  sem_close(hashReadySemaphore);

  // Wait for view to close to delete the sared memory
  shmdt(shm);
  shmctl(*shmid,IPC_RMID,NULL); 
  free(shmid);
  return 0;
}



void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]) {

// All slaves will read and write from the same pipes

  pid_t id;
  int i;
  for (i = 0; i < numberOfSlaves; i++)
  {
    id = fork();
    if (id == -1) {
      perror("Error en fork");
      exit(EXIT_FAILURE);
    } else if (id == 0) {

      // redirect filePipe to stdIn
      close(0);
      dup(filePipe[0]);
      close(filePipe[0]);
      close(filePipe[1]);
      // redirect hashPipe to stdOut
      close(hashPipe[0]);
      close(1);
      dup(hashPipe[1]);
      close(hashPipe[1]);

      // Load arguments
      char *args[2];
      args[0] = myPath;
      args[1] = NULL;
      execv("./slave.fl", args);
      perror("Error en exec");
      exit(EXIT_FAILURE);
    }
    slaveIds[i] = id;
  }

  close(filePipe[0]);
  close(hashPipe[1]);

  return;
}


void killChild(pid_t pid) {
    kill(pid,SIGKILL);
}

void killChilds(pid_t pids[], int numberOfSlaves) {
  int i;
  for (i = 0; i < numberOfSlaves; i++)
  {
    killChild(pids[i]);
  }
}

char * initSharedMemory(sem_t * shmReadySemaphore, int *shmid) {
  // share memory set up
  key_t key = ftok("./hashfiles.fl",1337); 
  if(key == -1){
    perror("ftok");
    exit(1);
  }
  
  *shmid = shmget(key,SHSIZE,IPC_CREAT | 0666);
  if(*shmid < 0){
    perror("shmget");
    exit(1);
  }

  char * shm = shmat(*shmid,NULL,0);

  if(shm == (char *)-1){
    perror("shmat");
    exit(1);
  }
  memset(shm, 0, SHSIZE);
  sem_post(shmReadySemaphore);
  return shm;
}

int sendFilesToSlaves(int totalFiles, int numberOfSlaves, int filesPerSlave, char *argv[], int filePipe[]) {
  
  int currSon = 0;
  int argIndex = 1;
  struct stat path_stat;
  int i;
  int remainingFiles = totalFiles;
  char *toPassString = calloc(FILENAMESIZE, FILENAMESIZE);
  // Sending files
  while(remainingFiles > 0) {

    // Write first wave of files ( N per Son)
    for (currSon = 0; currSon < numberOfSlaves; ++currSon) {
      for (i = 0; i < filesPerSlave; ++i) {
        if (remainingFiles > 0) {
          // fprintf(stderr, "remainingFiles: %d\n", remainingFiles);


          stat(argv[argIndex], &path_stat);
          if (S_ISDIR(path_stat.st_mode)) {
            totalFiles--;
          } else {
            strcpy(toPassString, argv[argIndex]);
            write(filePipe[1], toPassString, FILENAMESIZE);
            memset(toPassString,'\0',FILENAMESIZE);
          }   
          // Decrease remaining files and increase current arg
          remainingFiles--;
          argIndex++;

        }
      }
      // Write a separator
      write(filePipe[1], "\0", FILENAMESIZE);
    }

    // Release all the remaining files one at a time per son
    while (remainingFiles > 0) {
      // Write the file
      stat(argv[argIndex], &path_stat);
      
      if (S_ISDIR(path_stat.st_mode))
      {
        totalFiles--;
      } else {
        strcpy(toPassString, argv[argIndex]);
        write(filePipe[1], toPassString, FILENAMESIZE);
        memset(toPassString,'\0',FILENAMESIZE);
        // Write a separator
        write(filePipe[1], "\0", FILENAMESIZE);
      }
      remainingFiles--;
      argIndex++;
    }

  }

  free(toPassString);
  return totalFiles;

}


void sendHashesToOutputs(int totalFiles, int hashPipe[], char * shm , sem_t *hashReadySemaphore) {
  int shmOffset = 0;
  int readyFiles = 0;
  char *buf = calloc(BUFFERSIZE, BUFFERSIZE);
  FILE * fp;
  struct stat st = {0};

  if (stat("./outputs", &st) == -1) {
      mkdir("./outputs", 0700);
  }

  fp = fopen ("./outputs/hashFilesOutput","w");
  // All files are sent, now wait for files to complete
  while(readyFiles < totalFiles) {
    // Read from pipe
    read(hashPipe[0], buf, BUFFERSIZE);

    // Print to outputs
    fprintf (fp, "%s", buf);
    memcpy(shm + shmOffset,buf,BUFFERSIZE);
    // fprintf(stderr, "(padre)%s",buf);

    // Pointer change
    shmOffset+=BUFFERSIZE;
    if (shmOffset == SHSIZE) {
      shmOffset = 0;
    }
    sem_post(hashReadySemaphore);
    readyFiles++;
  }
  fclose (fp);
  free(buf);
}

void setSemaphoreValue(sem_t *sem, int value) {
  int semval, i;
  sem_getvalue(sem, &semval);
  int waits = semval - value;
  if (waits >= 0)
  {
    for (i = 0; i < waits; ++i)
    {
      sem_wait(sem);
    }
  } else {
    waits = waits * -1;
    for (i = 0; i < waits; ++i)
    {
      sem_post(sem);
    }
  }
}
