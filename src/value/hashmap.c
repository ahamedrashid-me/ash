#include <stdlib.h>
#include <string.h>
#include "value/hashmap.h"

static unsigned long hash_string(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

HashMap *map_new(void) {
    HashMap *m = malloc(sizeof(HashMap));
    m->capacity = 8;
    m->count = 0;
    m->entries = calloc(m->capacity, sizeof(HashMapEntry));
    return m;
}

static void map_resize(HashMap *m) {
    int old_capacity = m->capacity;
    HashMapEntry *old_entries = m->entries;
    m->capacity *= 2;
    m->entries = calloc(m->capacity, sizeof(HashMapEntry));
    m->count = 0;
    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].used) map_set(m, old_entries[i].key, old_entries[i].value);
    }
    free(old_entries);
}

void map_set(HashMap *m, char *key, Value value) {
    if ((double)(m->count + 1) / m->capacity > 0.7) map_resize(m);
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) {
            m->entries[h].value = value;
            free(key);
            return;
        }
        h = (h + 1) % (unsigned long)m->capacity;
    }
    m->entries[h].key = key;
    m->entries[h].value = value;
    m->entries[h].used = 1;
    m->count++;
}

int map_get(HashMap *m, const char *key, Value *out) {
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    unsigned long start = h;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) { *out = m->entries[h].value; return 1; }
        h = (h + 1) % (unsigned long)m->capacity;
        if (h == start) break;
    }
    return 0;
}

int map_has(HashMap *m, const char *key) {
    Value tmp;
    return map_get(m, key, &tmp);
}

void map_delete(HashMap *m, const char *key) {
    unsigned long h = hash_string(key) % (unsigned long)m->capacity;
    unsigned long start = h;
    while (m->entries[h].used) {
        if (strcmp(m->entries[h].key, key) == 0) {
            free(m->entries[h].key);
            m->entries[h].used = 0;
            m->count--;
            return;
        }
        h = (h + 1) % (unsigned long)m->capacity;
        if (h == start) break;
    }
}
