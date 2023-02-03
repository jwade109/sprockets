
all: node sync

node:
	gcc node.c -o node -g

clean:
	rm node

sync:
	./sync.sh pi@slingshot /home/pi/sprockets

.PHONY: node sync