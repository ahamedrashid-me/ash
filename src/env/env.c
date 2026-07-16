#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "env/env.h"
#include "interpreter/error.h"
#include "common.h"

typedef struct {
    char name[64];
    Value value;
} Variable;

typedef struct {
    Variable vars[MAX_VARS];
    int count;
} Scope;

static Scope scopes[MAX_SCOPES];
static int call_depth = 0;

void env_reset(void) {
    call_depth = 0;
    scopes[0].count = 0;
}

void env_push_scope(void) {
    if (call_depth + 1 >= MAX_SCOPES) {
        runtime_error("stack overflow (too much recursion)");
    }
    call_depth++;
    scopes[call_depth].count = 0;
}

void env_pop_scope(void) {
    call_depth--;
}

int env_get_depth(void) { return call_depth; }
void env_set_depth(int depth) { call_depth = depth; }

void set_variable(const char *name, int length, Value value) {
    Scope *s = &scopes[call_depth];
    for (int i = 0; i < s->count; i++) {
        if ((int)strlen(s->vars[i].name) == length && strncmp(s->vars[i].name, name, length) == 0) {
            s->vars[i].value = value;
            return;
        }
    }
    if (s->count >= MAX_VARS) {
        runtime_error("too many variables in scope");
    }
    strncpy(s->vars[s->count].name, name, length);
    s->vars[s->count].name[length] = '\0';
    s->vars[s->count].value = value;
    s->count++;
}

int try_get_variable(const char *name, int length, Value *out) {
    Scope *s = &scopes[call_depth];
    for (int i = 0; i < s->count; i++) {
        if ((int)strlen(s->vars[i].name) == length && strncmp(s->vars[i].name, name, length) == 0) {
            *out = s->vars[i].value;
            return 1;
        }
    }
    if (call_depth != 0) {
        Scope *g = &scopes[0];
        for (int i = 0; i < g->count; i++) {
            if ((int)strlen(g->vars[i].name) == length && strncmp(g->vars[i].name, name, length) == 0) {
                *out = g->vars[i].value;
                return 1;
            }
        }
    }
    return 0;
}

Value get_variable(const char *name, int length) {
    Value v;
    if (try_get_variable(name, length, &v)) return v;
    runtime_error("undefined variable: %.*s", length, name);
}

int env_snapshot_current_scope(char names_out[][64], Value *values_out, int max) {
    Scope *s = &scopes[call_depth];
    int n = s->count < max ? s->count : max;
    for (int i = 0; i < n; i++) {
        strcpy(names_out[i], s->vars[i].name);
        values_out[i] = s->vars[i].value;
    }
    return n;
}
