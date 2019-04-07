#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <string.h>


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

// Variable definitions
  char *buf;
  buf = calloc(bufferSize, bufferSize);
  char *fileName;
  fileName = calloc(fileNameSize,fileNameSize);
  char *fileNames[100];
  int currentFilename = 0;
  int processedFiles = 0;
  int i;
  while(1) {
    // Request file
    // Wait for pipe use
    sem_wait(filePipeReadySemaphore);

    // Read ALL The files in the pipe (until a read is null?)

    do {
      
      memset(fileName, 0, fileNameSize);
      read(STDIN_FILENO, fileName, fileNameSize);

      if (fileName[0] != '\0') {
        fileNames[currentFilename] = malloc(fileNameSize);
        strcpy(fileNames[currentFilename], fileName);
        currentFilename++;
      } else {

      }
    } while (fileName[0] != '\0');

    // Unlock the pipe
    sem_post(filePipeReadySemaphore);

    // Hash
    // fprintf(stderr, "processedFiles: %d, currentFilename: %d\n", processedFiles, currentFilename);
    for (i = processedFiles; i < currentFilename; ++i)
    {
      // fprintf(stderr, "i: %d. currentFilename: %d\n", i, currentFilename);
      hashFile(argv[0], fileNames[i], buf);
      if (buf[0] != 0)
      {
        // Return to parent
        write(1, buf,bufferSize);
      }
      // Cleanup
      // fprintf(stderr, "%d: %s\n", getpid(), buf);
      memset(buf,0,bufferSize);
      processedFiles++;
      free(fileNames[i]);
    }

  }
  free(fileName);
  free(buf);

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

// Redirect stdOut to pipe
    close(1);
    // close(2);
    dup(p[1]);
    // dup(p[1]);

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