all: clean mypingsrv mypingcli zigzag

mypingsrv:
	gcc -g -Wall -o mypingsrv mypingsrv.c

mypingcli:
	gcc -g -Wall -o mypingcli mypingcli.c

zigzag:
	gcc -g -Wall -o zigzagconf zigzagconf.c security.c
	gcc -g -Wall -o zigzagrouter zigzagrouter.c security.c

clean:
	rm -f mypingsrv
	rm -f mypingcli
	rm -f zigzagconf
	rm -f zigzagrouter

