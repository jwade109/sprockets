
C_FLAGS=-std=gnu11 -Wfatal-errors -Wdouble-promotion -Wfloat-conversion -Werror=implicit-function-declaration -Werror=format -Wall -Wextra -Wpedantic -g

LIB_COMPILATION_UNITS=common ring_buffer dynamic_array
LIB_OBJECTS=$(addsuffix .o, $(addprefix build/, ${LIB_COMPILATION_UNITS}))
LIB_HEADERS=include/common.h include/ring_buffer.h

MESSAGES=packet
MSG_OBJECTS=$(addsuffix .o, $(addprefix build/, ${MESSAGES}))
MSG_HEADERS=$(addsuffix .h, $(addprefix include/, ${MESSAGES}))

all:
	@make --no-print-directory -j8 node msggen messages

build/%.o: src/%.c ${LIB_HEADERS}
	@mkdir -p build/
	gcc src/$*.c ${C_FLAGS} -c -o build/$*.o -Iinclude/

node: ${LIB_OBJECTS} ${MSG_OBJECTS} build/node.o
	gcc ${LIB_OBJECTS} ${MSG_OBJECTS} build/node.o -o node

msggen: ${LIB_OBJECTS} build/msggen.o
	gcc ${LIB_OBJECTS} build/msggen.o -o msggen

messages: msggen
	./msggen msg/packet.msg include/packet.h src/packet.c

clean:
	rm -rf build/ node msggen
