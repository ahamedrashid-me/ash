CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
LDFLAGS = -lm

ASHC_SRC = src/main.c src/lexer/lexer.c src/value/value.c src/value/hashmap.c src/env/env.c \
           src/interpreter/parser_state.c src/interpreter/expressions.c \
           src/interpreter/statements.c src/interpreter/functions.c src/interpreter/closures.c src/interpreter/error.c \
           src/builtins/builtins.c
ASHC_OBJ = $(ASHC_SRC:.c=.o)

ASHVM_SRC = src/vm_main.c src/lexer/lexer.c src/vm/value.c src/vm/hashmap.c src/vm/builtins.c src/vm/chunk.c src/vm/compiler.c src/vm/vm.c
ASHVM_OBJ = $(ASHVM_SRC:.c=.o)

all: ashc ashvm

ashc: $(ASHC_OBJ)
	$(CC) $(CFLAGS) -o ashc $(ASHC_OBJ) $(LDFLAGS)

ashvm: $(ASHVM_OBJ)
	$(CC) $(CFLAGS) -o ashvm $(ASHVM_OBJ) $(LDFLAGS)

clean:
	find src -name '*.o' -delete
	rm -f ashc ashvm
