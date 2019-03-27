#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main (int argc, char *argv[]){
  int i;
  for (i = 0; i < argc; i++) { 
    printf("%s\n", argv[i]);
  }

  return 0;
}