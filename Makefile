
C_FLAGS=-std=gnu11 -Wfatal-errors -Wdouble-promotion -Wfloat-conversion \
	-Werror=implicit-function-declaration -Werror=format \
	-Werror=return-type -Werror=format-extra-args \
	-Wall -Wextra -Wpedantic -g

LIB_COMPILATION_UNITS=common ring_buffer dynamic_array datetime
LIB_OBJECTS=$(addsuffix .o, $(addprefix build/, ${LIB_COMPILATION_UNITS}))
LIB_HEADERS=include/common.h include/ring_buffer.h

MESSAGES=timestamp vec3
MSG_OBJECTS=$(addsuffix .o, $(addprefix build/msg/,   ${MESSAGES}))
MSG_HEADERS=$(addsuffix .h, $(addprefix include/msg/, ${MESSAGES}))

all: node

messages: ${MSG_OBJECTS}

build/%.o: src/%.c ${LIB_HEADERS} messages
	@mkdir -p build/
	gcc src/$*.c ${C_FLAGS} -c -o build/$*.o -I include/

build/msg/%.o: msggen msg/%.msg
	@echo " -- MSG[$*] BUILD --"
	@mkdir -p build/msg/ src/msg/ include/msg/
	./msggen msg/$*.msg include/msg/$*.h src/msg/$*.c $*
	gcc src/msg/$*.c ${C_FLAGS} -c -o build/msg/$*.o -I include/
	@echo " -- MSG[$*] FINISHED --"

node: ${MSG_OBJECTS} ${LIB_OBJECTS} build/node.o
	gcc ${LIB_OBJECTS} ${MSG_OBJECTS} build/node.o -o node

msggen: src/dynamic_array.c src/msggen.c include/dynamic_array.h
	gcc ${C_FLAGS} src/dynamic_array.c src/msggen.c -I include/ -o msggen

clean:
	rm -rf build/ node msggen include/msg/ src/msg/
