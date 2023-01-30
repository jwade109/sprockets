
all: client server

client:
	gcc client.c -o client

server:
	gcc server.c -o server

sync:
	./sync.sh

clean:
	rm client server

.PHONY: client server sync