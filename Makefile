
all: node

node:
	gcc node.c -o node -g

clean:
	rm node

.PHONY: node