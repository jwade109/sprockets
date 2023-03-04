
C_FLAGS=-std=gnu11 -Wfatal-errors -Wdouble-promotion -Wfloat-conversion -Werror=implicit-function-declaration -Werror=format -Wall -Wextra -Wpedantic -g

LIB_COMPILATION_UNITS=common ring_buffer dynamic_array
LIB_OBJECTS=$(addsuffix .o, $(addprefix build/, ${LIB_COMPILATION_UNITS}))
LIB_HEADERS=include/common.h include/ring_buffer.h

MESSAGES=packet timestamp vec3
MSG_OBJECTS=$(addsuffix .o, $(addprefix build/msg/,   ${MESSAGES}))
MSG_HEADERS=$(addsuffix .h, $(addprefix include/msg/, ${MESSAGES}))

messages: ${MSG_OBJECTS}

all:
	@make --no-print-directory -j8 messages node

build/%.o: src/%.c ${LIB_HEADERS}
	@mkdir -p build/
	gcc src/$*.c ${C_FLAGS} -c -o build/$*.o -I include/

build/msg/%.o: msggen msg/%.msg
	@mkdir -p build/msg/ src/msg/ include/msg/
	./msggen msg/$*.msg include/msg/$*.h src/msg/$*.c $*
	gcc src/msg/$*.c ${C_FLAGS} -c -o build/msg/$*.o -I include/

node: ${MSG_OBJECTS} ${LIB_OBJECTS} build/node.o
	gcc ${LIB_OBJECTS} ${MSG_OBJECTS} build/node.o -o node

msggen: build/dynamic_array.o build/msggen.o
	gcc build/dynamic_array.o build/msggen.o -o msggen

clean:
	rm -rf build/ node msggen include/msg/ src/msg/
