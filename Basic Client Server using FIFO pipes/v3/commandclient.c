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
	int fd1,fd2;
	char fifoname[100];
	char buf[100];
	char bufpass[10000];
	char pid[100];
        char clientoutput[5000] = "";

	// converting process id to string
	sprintf(pid,"%d",getpid());

	// initializing client FIFO name
        char clientfifoname[100] = "cfifo";
	// concatenation process id to complete client fifo name
	strcat(clientfifoname,pid);
	
	FILE *fptr;

	// opening serverfifo,dat file and reading input file name
	if((fptr = fopen("serverfifo.dat","r")) == NULL)
	{
		fprintf(stdout,"Unable to open file");
		exit(1);
	}
	else
	{
		fgets(fifoname,100,fptr);
		fclose(fptr);
	}

	mkfifo(clientfifoname,0777);
	while(1)
	{
		// refreshing output string and other buffers for every iteration of the loop
		memset(clientoutput,0,5000);
		fflush(stdout);
		fflush(stdin);

		// bufpass will contain the process id and the user input. This will be written to the server FIFO
		strcpy(bufpass,pid);
		strcat(bufpass,"\n");
		// opening FIFO
		fd1 = open(fifoname,O_WRONLY);
		// printing prompt
                fprintf(stdout,">");
		// taking input from stdin
		fgets(buf,100,stdin);

		// checking that user input string does not exceed pipe buffer
		if(strlen(buf) > PIPE_BUF)
                {
                        fprintf(stderr,"Buffer size exceeded for the input, cannot proceed for this input");
                }
                else
		{
		// making the final string to be passed to the server FIFO
		strcat(bufpass,buf);

		// writing to FIFO (named pipe)
		write(fd1,bufpass,strlen(bufpass)+1);
		}
		close(fd1);

		// Reading the server output from client FIFO
		fd2 = open(clientfifoname,O_RDONLY);
                read(fd2,clientoutput,10000);
		close(fd2);

		fprintf(stdout,"%s",clientoutput);

	}
        return 0;

}
