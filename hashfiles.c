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

#define SHSIZE 100

void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]);
void killChild(pid_t pid);
void killChilds(pid_t pids[], int numberOfSlaves);


static const int bufferSize = 512;
static const int fileNameSize = 512;

int main (int argc, char *argv[]){
  

  // Init variables
  int numberOfSlaves = 1;
  int filesPerSlave = 2;
  pid_t slaveIds[numberOfSlaves];
  int totalFiles = argc - 1;
  int remainingFiles = totalFiles;
  int readyFiles = 0;

  int argIndex = 1;
  int i, currSon;
  char *buf = calloc(bufferSize, bufferSize);
  char *toPassString = calloc(fileNameSize, fileNameSize);
  buf = calloc(bufferSize, bufferSize);


  // share memory set up
  key_t key = ftok("./hashfiles.fl",1337); 
  if(key == -1){
    perror("ftok");
    exit(1);
  }
  int shmid;
  char  * shm;
  char * s;
  

  shmid = shmget(key,SHSIZE,IPC_EXCL | IPC_CREAT);
  if(shmid < 0){
    perror("shmget");
    exit(1);
  }

  shm = shmat(shmid,NULL,0);
  memset(shm, 0, SHSIZE);
  if(shm == (char *)-1){
    perror("shmat");
    exit(1);
  }
  
  // Init pipes
  int hashPipe[2], filePipe[2];
  pipe(hashPipe);
  pipe(filePipe);

  // Semaphore init
  // Closing the semaphore to avoid previous non-closed semaphores conflicts
  sem_unlink("filePipeReadySemaphore");
  sem_t *filePipeReadySemaphore = sem_open("filePipeReadySemaphore", O_CREAT, 0644, 1);
  // Create slaves
  createSlaves(argv[0], numberOfSlaves, hashPipe, filePipe, slaveIds);

  // Sending initial load
  while(remainingFiles > 0) {

    // Write first wave of files ( N per Son)
    for (currSon = 0; currSon < numberOfSlaves; ++currSon)
    {
      for (i = 0; i < filesPerSlave; ++i)
      {
        if (remainingFiles > 0) {
          // fprintf(stderr, "remainingFiles: %d\n", remainingFiles);

          strcpy(toPassString, argv[argIndex]);
          write(filePipe[1], toPassString, fileNameSize);
          memset(toPassString,'\0',fileNameSize);
          // Decrease remaining files and increase current arg
          remainingFiles--;
          argIndex++;

        }
      }
      // Write a separator
      write(filePipe[1], "\0", fileNameSize);
    }

    // Release all the remaining files one at a time per son
    while (remainingFiles > 0) {
      // Write the file
      strcpy(toPassString, argv[argIndex]);
      write(filePipe[1], toPassString, fileNameSize);
      memset(toPassString,'\0',fileNameSize);
      // Write a separator
      write(filePipe[1], "\0", fileNameSize);
      remainingFiles--;
      argIndex++;
    }
  }


  // All files are sent, now wait for files to complete
  sleep(5); // wait for view
  fprintf(stderr, "%s\n", "now...");
  while(readyFiles < totalFiles) {
    read(hashPipe[0], buf, bufferSize);
    readyFiles++;
    fprintf(stderr, "(padre)%s",buf);
    memcpy(s,buf,bufferSize);
    s+=bufferSize;
  }


  // Cleanup
  killChilds(slaveIds, numberOfSlaves);
  close(hashPipe[0]);
  close(filePipe[1]);
  free(buf);
  free(toPassString);
  sem_close(filePipeReadySemaphore);
  sem_unlink("filePipeReadySemaphore");
  shmdt(shm);



  // destroy the shared memory 
  // shmctl(shmid,IPC_RMID,NULL); 
   
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