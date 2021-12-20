all : commandserver.bin commandclient.bin

commandserver.bin : commandserver.c parsingfunction.c
	gcc -o commandserver.bin commandserver.c parsingfunction.c

commandclient.bin : commandclient.c
	gcc -o commandclient.bin commandclient.c 
