#include <stdlib.h>
#include "vm/chunk.h"

void chunk_init(Chunk *c) {
    c->count = 0; c->capacity = 0; c->code = NULL;
    c->const_count = 0; c->const_capacity = 0; c->constants = NULL;
}

int chunk_write(Chunk *c, uint8_t byte) {
    if (c->count >= c->capacity) {
        c->capacity = c->capacity == 0 ? 64 : c->capacity * 2;
        c->code = realloc(c->code, c->capacity);
    }
    c->code[c->count] = byte;
    return c->count++;
}

int chunk_add_constant(Chunk *c, VMValue value) {
    if (c->const_count >= c->const_capacity) {
        c->const_capacity = c->const_capacity == 0 ? 16 : c->const_capacity * 2;
        c->constants = realloc(c->constants, c->const_capacity * sizeof(VMValue));
    }
    c->constants[c->const_count] = value;
    return c->const_count++;
}
