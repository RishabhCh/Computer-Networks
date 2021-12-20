// Simple shell example using fork() and execlp().

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include "parsingfunction.h"

int main(void)
{
pid_t k;
char buf[100];
int status;
int len;

  while(1) {

	int j=0;

	char *args[10];
	char *ptr;

	// print prompt
  	fprintf(stdout,"[%d]$ ",getpid());

	// read command from stdin
	fgets(buf, 100, stdin);
    
    	len = strlen(buf);
        if(len == 1)                            // only return key pressed
          continue;
        buf[len-1] = '\0';
        
	// function to parse the command and store in a character pointer array - args
	parsingfunction(buf,args);

  	k = fork();
  	if (k==0) {
  	// child code
    	  if(execvp(args[0],args) == -1)	// if execution failed, terminate child
	  	exit(1);
  	}
  	else {
  	// parent code 
	  
	waitpid(k, &status, 0);
  	}

  }
}
