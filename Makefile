
all: node

node:
	gcc -std=gnu11 node.c -o node -g -Wall -Wextra -Wpedantic \
        -Wfatal-errors -Wdouble-promotion -Wfloat-conversion \
        -Werror=implicit-function-declaration \
		-Werror=format

clean:
	rm node

sync:
	./sync.sh pi@slingshot /home/pi/sprockets

.PHONY: node sync
