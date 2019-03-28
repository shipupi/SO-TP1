#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>


static const char md5Path[] = "/usr/bin/md5sum";
static const int bufferSize = 2000;
static const int fileNameSize = 500;


int hashFile(char *myPath, char *pathToFile, char *buffer);


/*
Pipes in slave:


0 FILEPIPE    R
1 HASHPIPE    W
2 STDERR      W
3 REQUESTPIPE W

*/


int main (int argc, char *argv[]){

// Semaphore init
  sem_t *pipeReadySemaphore = sem_open("pipeReadySemaphore", O_RDWR);
  sem_t *fileAvailableSemaphore = sem_open("fileAvailableSemaphore", O_RDWR);

// Variable definitions
  char *buf;
  char *fileName;
  fileName = calloc(fileNameSize,fileNameSize);
  buf = calloc(bufferSize, bufferSize);

  while(1) {
    // Request file
    // printf("Slave Requesting File\n");
    write(3,"",1);

    // Wait for an available file
    sem_wait(fileAvailableSemaphore);
    read(STDIN_FILENO, fileName, fileNameSize);
    sem_post(pipeReadySemaphore);
    // printf("hashing file: %s\n", fileName); 
    hashFile(argv[0], fileName, buf);
    // printf("File is hashed: %s\n", buf);
    write(1, buf,bufferSize);
  }

// Free resources
  free(buf);
  free(fileName);

  return 0;
}

int hashFile(char *myPath, char *pathToFile, char *buffer) {
// Creating Pipe
  int p[2];
  pid_t id;
  pipe(p);
  int status;

  id = fork();

  if (id == -1) {
    perror("Error en fork");
    exit(EXIT_FAILURE);
  } else if (id == 0) {

// HIJO

// Redirect stdOut to pipe
    close(1);
    dup(p[1]);
    close(p[0]);
    close(p[1]);

// Load arguments
    char *args[3];
    args[0] = myPath;
    args[1] = pathToFile;
    args[2] = NULL;
    // fprintf(stderr, "%s\n", myPath);
    // fprintf(stderr, "%s\n", pathToFile);
    execv(md5Path, args);
    perror("Error en exec");
    exit(EXIT_FAILURE);
  }

  close(p[1]);
  wait(&status);
  read(p[0], buffer, bufferSize);
  close(p[0]);

  return 0;
}