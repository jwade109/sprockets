
all: client server

client:
	gcc client.c -o client -g

server:
	gcc server.c -o server -g

sync:
	./sync.sh

clean:
	rm client server

.PHONY: client server sync