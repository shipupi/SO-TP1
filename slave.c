#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>


static const char md5Path[] = "/usr/bin/md5sum";
static const int bufferSize = 512;
static const int fileNameSize = 512;


int hashFile(char *myPath, char *pathToFile, char *buffer);


/*
Pipes in slave:


0 FILEPIPE    R ( Receives file names to hash )
1 HASHPIPE    W ( Sends hash values back to parent)
2 STDERR      W ( For debug )
3 REQUESTPIPE W ( Signal that child is ready to hash)

*/


int main (int argc, char *argv[]){

// Semaphore init
  sem_t *filePipeReadySemaphore = sem_open("filePipeReadySemaphore", O_RDWR);
  sem_t *fileAvailableSemaphore = sem_open("fileAvailableSemaphore", O_RDWR);

// Variable definitions
  char *buf;
  char *fileName;

  while(1) {
    fileName = calloc(fileNameSize,fileNameSize);
    buf = calloc(bufferSize, bufferSize);
    // Request file
    // printf("Slave Requesting File\n");
    write(3,"",1);

    // Wait for an available file
    sem_wait(fileAvailableSemaphore);
    read(STDIN_FILENO, fileName, fileNameSize);
    sem_post(filePipeReadySemaphore);
    hashFile(argv[0], fileName, buf);
    write(1, buf,bufferSize);
    free(buf);
    free(fileName);
  }

// Free resources
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

// Redirect stdOut to pipe and stdErr to pipe
    close(1);
    close(2);
    dup(p[1]);
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