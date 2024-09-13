default: server

server: server.c server.h utils.o
	gcc server.c -lcrypto -Xlinker utils.o -o $@

utils.o: utils.c utils.h
	gcc -c utils.c 

.PHONY clean: server utils.o
	rm server utils.o