#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>

static const char md5Path[] = "/usr/bin/md5sum";
static const int bufferSize = 1000;
static const int fileNameSize = 200;


int hashFile(char *myPath, char *pathToFile, char *buffer);


int main (int argc, char *argv[]){

	// Variable definitions
	char *buf;
	char *fileName;
	fileName = malloc(fileNameSize);
	buf = malloc(bufferSize);

	read(STDIN_FILENO, fileName, fileNameSize);
	hashFile(argv[0], fileName, buf);
	printf("%s\n", buf);

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
		printf("%s\n", myPath);
		printf("%s\n", pathToFile);
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