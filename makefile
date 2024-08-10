default: server

server: server.c
	gcc server.c -lcrypto -o $@

.PHONY clean: server
	rm server