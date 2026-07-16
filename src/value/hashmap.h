#ifndef ASH_HASHMAP_H
#define ASH_HASHMAP_H
#include "value/value.h"

typedef struct {
    char *key;
    Value value;
    int used;
} HashMapEntry;

struct HashMap {
    HashMapEntry *entries;
    int count;
    int capacity;
};

HashMap *map_new(void);
void map_set(HashMap *m, char *key, Value value); // takes ownership of key
int map_get(HashMap *m, const char *key, Value *out);
int map_has(HashMap *m, const char *key);
void map_delete(HashMap *m, const char *key);

#endif
