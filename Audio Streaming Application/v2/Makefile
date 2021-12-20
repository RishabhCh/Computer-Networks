all: audiosrv audiocli

audiosrv: utility.h audiosrv.c
	gcc audiosrv.c -o audiosrv

audiocli: utility.h audiocli.c
	gcc audiocli.c -pthread -lasound -o audiocli
