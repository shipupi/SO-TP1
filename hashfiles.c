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

#define SHSIZE 30720

void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]);
void killChild(pid_t pid);
void killChilds(pid_t pids[], int numberOfSlaves);


static const int bufferSize = 512; // IMPORTANT: SHSIZE must be divisible by bufferSize
static const int fileNameSize = 512;

int main (int argc, char *argv[]){

  // Init View shared variables (and clean semaphores)
  printf("%d\n", getpid());  
  fflush(stdout);
  sem_t *hashReadySemaphore = sem_open("hashReadySemaphore", O_CREAT, 0644, 0);
  sem_t *shmReadySemaphore = sem_open("shmReadySemaphore", O_CREAT, 0644, 0);
  int semval, i;

  sem_getvalue(hashReadySemaphore, &semval);
  for (i = 0; i < semval; ++i)
  {
    sem_wait(hashReadySemaphore);
  }
  sem_getvalue(shmReadySemaphore, &semval);
  for (i = 0; i < semval; ++i)
  {
    sem_wait(shmReadySemaphore);
  }

  sleep(1); // wait for view

  // Init variables
  int numberOfSlaves = 2;
  int filesPerSlave = 1;
  pid_t slaveIds[numberOfSlaves];
  int totalFiles = argc - 1;
  int currSon;
  int remainingFiles = totalFiles;
  int readyFiles = 0;
  struct stat path_stat;
  int argIndex = 1;
  char *buf = calloc(bufferSize, bufferSize);
  char *toPassString = calloc(fileNameSize, fileNameSize);
  FILE * fp;
  fp = fopen ("./outputs/hashFilesOutput","w");

  // share memory set up
  key_t key = ftok("./hashfiles.fl",1337); 
  if(key == -1){
    perror("ftok");
    exit(1);
  }
  int shmid;
  char  * shm;
  int shmOffset = 0;
  
  shmid = shmget(key,SHSIZE,IPC_CREAT | 0666);
  if(shmid < 0){
    perror("shmget");
    exit(1);
  }

  shm = shmat(shmid,NULL,0);

  if(shm == (char *)-1){
    perror("shmat");
    exit(1);
  }
  memset(shm, 0, SHSIZE);
  sem_post(shmReadySemaphore);
  

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


          stat(argv[argIndex], &path_stat);
          if (S_ISDIR(path_stat.st_mode)) {
            totalFiles--;
          } else {
            strcpy(toPassString, argv[argIndex]);
            write(filePipe[1], toPassString, fileNameSize);
            memset(toPassString,'\0',fileNameSize);
          }   
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
      stat(argv[argIndex], &path_stat);
      
      if (S_ISDIR(path_stat.st_mode))
      {
        totalFiles--;
      } else {
        strcpy(toPassString, argv[argIndex]);
        write(filePipe[1], toPassString, fileNameSize);
        memset(toPassString,'\0',fileNameSize);
        // Write a separator
        write(filePipe[1], "\0", fileNameSize);
      }
      remainingFiles--;
      argIndex++;
    }
  }


  // All files are sent, now wait for files to complete
  
  while(readyFiles < totalFiles) {
    // Read from pipe
    read(hashPipe[0], buf, bufferSize);

    // Print to outputs
    fprintf (fp, "%s", buf);
    memcpy(shm + shmOffset,buf,bufferSize);
    // fprintf(stderr, "(padre)%s",buf);

    // Pointer change
    shmOffset+=bufferSize;
    if (shmOffset == SHSIZE) {
      shmOffset = 0;
    }
    sem_post(hashReadySemaphore);
    readyFiles++;
  }


  // Cleanup
  killChilds(slaveIds, numberOfSlaves);
  close(hashPipe[0]);
  close(filePipe[1]);
  free(buf);
  free(toPassString);
  sem_close(filePipeReadySemaphore);
  sem_unlink("filePipeReadySemaphore");
  sem_close(shmReadySemaphore);
  shmdt(shm);
  fclose (fp);
  sem_close(hashReadySemaphore);
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