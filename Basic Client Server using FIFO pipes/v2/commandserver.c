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
	char buf[100];
	int status;
	int len;
	int fd1;

	FILE *fptr;
	char *myfifo = "serverfifo";
        char fifoname[100]; // to store fifo file name for writing
        
        strcpy(fifoname,myfifo);

	// opening serverfifo.dat file
	fptr = fopen("serverfifo.dat","w");

	// creating FIFO with all permissions (which can be read, written and executed by everyone)
	mkfifo(myfifo,0777);
	
	// writing fifo file name in serverfifo.dat
	fputs(fifoname,fptr);
	// close and save serverfifo.dat
	fclose(fptr);

	while(1)
	{
		// opening the FIFO
		fd1 = open(myfifo,O_RDONLY);
		// reading the client input into FIFO
		read(fd1,buf,100);
		// closing the FIFO file
		close(fd1);

                char *args[10];


                len = strlen(buf);
                if(len == 1)                            // only return key pressed
                  continue;
                buf[len-1] = '\0';

                parsingfunction(buf,args);

                k = fork();
                if (k==0) 
		{
                	// child code
          		if(execvp(args[0],args) == -1)        // if execution failed, terminate child
                	exit(1);
        	}
        	else 
		{
        		// parent code
        		waitpid(k, &status, 0);
        	}

	}


	return 0;

}
