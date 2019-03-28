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

void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[], int requestPipe[],pid_t slaveIds[]);
void killChild(pid_t pid);
void killChilds(pid_t pids[], int numberOfSlaves);


static const int bufferSize = 2000;

int main (int argc, char *argv[]){
  

  // Init variables
  int numberOfSlaves = 1;
  pid_t slaveIds[numberOfSlaves];

  int totalFiles = argc - 1;
  int remainingFiles = totalFiles;
  int readyFiles = 0;

  int argIndex = 1;
  int i;
  char *tmp[1];
  char *buf;
  buf = calloc(bufferSize, bufferSize);
  // Init pipes
  int hashPipe[2], filePipe[2], requestPipe[2];
  pipe(requestPipe);
  pipe(hashPipe);
  pipe(filePipe);


  printf("Printing arguments for test: \n\n");
  for (i = 0; i < argc; i++) { 
    printf("%s\n", argv[i]);
  }
  printf("\n");
  // Semaphore init
  sem_t *pipeReadySemaphore = sem_open("pipeReadySemaphore", O_CREAT, 0644, 0);
  sem_t *fileAvailableSemaphore = sem_open("fileAvailableSemaphore", O_CREAT, 0644, 0);
  sem_post(pipeReadySemaphore);
  // Create slaves
  createSlaves(argv[0], numberOfSlaves, hashPipe, filePipe, requestPipe, slaveIds);



  // Main Loop
  printf("Remaining files: %d\n", remainingFiles);

  while(remainingFiles > 0) {
    // Wait for file request
    read(requestPipe[0], tmp, 1);

    // Wait for pipe to be ready
    printf("A file was requested\n");
    sem_wait(pipeReadySemaphore);
    printf("Pipe is Ready! Writing to filePipe\n");

    // Write into the filepipe
    write(filePipe[1], argv[argIndex], strlen(argv[argIndex]) + 1);

    // Post to the fileavailable semaphore
    sem_post(fileAvailableSemaphore);

    // Decrease remaining files
    remainingFiles--;
    argIndex++;
    printf("Remaining files: %d\n", remainingFiles);
  }


  while(readyFiles < totalFiles) {
    read(hashPipe[0], buf, bufferSize);
    readyFiles++;
    printf("%s",buf);
  }


  // Cleanup
  killChilds(slaveIds, numberOfSlaves);
  close(hashPipe[0]);
  close(filePipe[1]);
  free(buf);

  return 0;
}



void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[], int requestPipe[],pid_t slaveIds[]) {

// All slaves will read and write from the same pipes

  pid_t id;
  for (int i = 0; i < numberOfSlaves; i++)
  {
    id = fork();
    if (id == -1) {
      perror("Error en fork");
      exit(EXIT_FAILURE);
    } else if (id == 0) {
      // redirect filePipe to stdIn
      close(0);
      dup(filePipe[0]);
      close(requestPipe[0]);
      dup(requestPipe[1]);
      close(requestPipe[1]);
      close(filePipe[0]);
      close(filePipe[1]);
      close(hashPipe[0]);
      // redirect hashPipe to stdOut
      // For testing purposes, leave stdOut to check if everything is working correctly
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