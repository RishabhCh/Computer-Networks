#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <limits.h>

int main()
{
	int fd1;
	char fifoname[100];
	char buf[100];
        
	FILE *fptr;

	// opening serverfifo.dat file
	if((fptr = fopen("serverfifo.dat","r")) == NULL)
	{
		fprintf(stdout,"Unable to open file");
		exit(1);
	}
	else
	{
		// getting the server fifo name
		fgets(fifoname,100,fptr);
		fclose(fptr);
	}

	while(1)
	{
		// opening server FIFO
		fd1 = open(fifoname,O_WRONLY);
		// printing prompt
                fprintf(stdout,">");
		// taking user input
		fgets(buf,100,stdin);
		if(strlen(buf) > PIPE_BUF)
		{
			fprintf(stderr,"Buffer size exceeded for the input, cannot proceed for this input");
		}
		else
		{
			// writing user input into fifo file
			write(fd1,buf,strlen(buf)+1);
		}
		// closing FIFO
		close(fd1);

	}
        return 0;

}
