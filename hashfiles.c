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

void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]);
void killChild(pid_t pid);
void killChilds(pid_t pids[], int numberOfSlaves);


static const int bufferSize = 512;
static const int fileNameSize = 512;

int main (int argc, char *argv[]){
  

  // Init variables
  int numberOfSlaves = 5;
  int filesPerSlave = 3;
  pid_t slaveIds[numberOfSlaves];
  int totalFiles = argc - 1;
  int remainingFiles = totalFiles;
  int readyFiles = 0;

  int argIndex = 1;
  int i, currSon;
  char *buf;
  buf = calloc(bufferSize, bufferSize);
  // Init pipes
  int hashPipe[2], filePipe[2];
  pipe(hashPipe);
  pipe(filePipe);


  printf("Printing arguments for test: \n\n");
  for (i = 0; i < argc; i++) { 
    printf("%s\n", argv[i]);
  }
  printf("\n");
  // Semaphore init
  // Lo cierro por las dudas para no tener conflictos con alguno q estaba existiendo de antes
  sem_unlink("filePipeReadySemaphore");
  sem_t *filePipeReadySemaphore = sem_open("filePipeReadySemaphore", O_CREAT, 0644, 1);
  // Create slaves
  createSlaves(argv[0], numberOfSlaves, hashPipe, filePipe, slaveIds);



  // Main Loop
  // printf("Remaining files: %d\n", remainingFiles);
  int son = 0;

  while(remainingFiles > 0) {

    // Write first wave of files ( N per Son)
    for (currSon = 0; currSon < numberOfSlaves; ++currSon)
    {
      for (i = 0; i < filesPerSlave; ++i)
      {
        if (remainingFiles > 0) {
          // fprintf(stderr, "remainingFiles: %d\n", remainingFiles);
          write(filePipe[1], argv[argIndex], fileNameSize);
          // Decrease remaining files and increase current arg
          fprintf(stderr, "Sent to son %d: %s\n", son, argv[argIndex]);
          remainingFiles--;
          argIndex++;
        }
      }
      // Write a separator
      write(filePipe[1], "\0", fileNameSize);
      son++;
    }

    // Release all the remaining files one at a time per son
    while (remainingFiles > 0) {
      // Write the file
      write(filePipe[1], argv[argIndex], fileNameSize);
      // Write a separator
      write(filePipe[1], "\0", fileNameSize);
      fprintf(stderr, "Sent to son %d: %s\n", son, argv[argIndex]);
      son++;
      remainingFiles--;
      argIndex++;
    }
  }


  // All files are sent, now wait for files to complete

  while(readyFiles < totalFiles) {
    read(hashPipe[0], buf, bufferSize);
    readyFiles++;
    // printf("%s",buf);
  }


  // Cleanup
  killChilds(slaveIds, numberOfSlaves);
  close(hashPipe[0]);
  close(filePipe[1]);
  free(buf);
  sem_close(filePipeReadySemaphore);
  sem_unlink("filePipeReadySemaphore");

  return 0;
}



void createSlaves(char *myPath, int numberOfSlaves, int hashPipe[], int filePipe[],pid_t slaveIds[]) {

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