#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parsingfunction.h"

int main()
{
	pid_t k;
	char buf[10000];
	int status;
	int len;
	int fd1,fd2;

	FILE *fptr;
	char *myfifo = "serverfifo";    // name of the server fifo
        char fifoname[100];             // to store fifo file name for writing

        // setting serverfifo name
        strcpy(fifoname,myfifo);

	// opening serverfifo.dat file
	fptr = fopen("serverfifo.dat","w");

	// creating FIFO with all permissions (which can be read, written and executed by everyone)
	mkfifo(myfifo,0777);
	
	// writing fifo file name in serverfifo.dat
	fputs(fifoname,fptr);

	// close and save serverfifo.dat
	fclose(fptr);

	char *pidargs[2]; // to store the two lines of client input
	char *cltfifo; // to store the name of client FIFO
	while(1)
	{
		char clientfifoname[100] = "cfifo";
                
                // opening serverfifo in read mode
		fd1 = open(myfifo,O_RDONLY);
                // reading serverfifo input
		read(fd1,buf,10000);
                // closing serverfifo
		close(fd1);
                
                
		len = strlen(buf);
                if(len == 1)                            // only return key pressed
                  continue;
                buf[len-1] = '\0';

                // getting pid and user input from server fifo (splitting string using \n delimiter)
                parsingfunction(buf,"\n",pidargs);

		// storing client fifo name
		cltfifo = strcat(clientfifoname,pidargs[0]);

                char *args[10];

                // parsing user input
                parsingfunction(pidargs[1]," ",args);
          
		
                k = fork();
                if (k==0) 
		{
                // child code

                // opening client thread
		fd2 = open(cltfifo,O_WRONLY);
                // setting the write path to client FIFO
		dup2(fd2,1);
                // closing client FIFO
                close(fd2);
                
                // executing linux command
          	if(execvp(args[0],args) == -1)        // if execution failed, terminate child
                exit(1);
        	}
        	else {
        	// parent code
        	waitpid(k, &status, 0);
        	}

	}


	return 0;

}
