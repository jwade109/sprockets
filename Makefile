
C_FLAGS=-std=gnu11 -Wfatal-errors -Wdouble-promotion -Wfloat-conversion -Werror=implicit-function-declaration -Werror=format -Wall -Wextra -Wpedantic -g

COMPILATION_UNITS=common ring_buffer
OBJECT_FILES=$(addsuffix .o, $(addprefix build/, ${COMPILATION_UNITS}))
INCLUDE_FILES=include/common.h include/ring_buffer.h

all:
	@make --no-print-directory -j8 node multiserver

build/%.o: src/%.c ${INCLUDE_FILES}
	mkdir -p build/
	gcc src/$*.c ${C_FLAGS} -c -o build/$*.o -Iinclude/

node: ${OBJECT_FILES} build/node.o
	gcc ${OBJECT_FILES} build/node.o -o node

multiserver: ${OBJECT_FILES} build/multiserver.o
	gcc ${OBJECT_FILES} build/multiserver.o -o multiserver -lm

clean:
	rm -rf build/ node
