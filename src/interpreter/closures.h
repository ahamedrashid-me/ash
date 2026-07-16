#ifndef ASH_CLOSURES_H
#define ASH_CLOSURES_H

#include "value/value.h"
#include "common.h"

#define MAX_CAPTURED MAX_VARS

struct Closure {
    const char *body_start;
    char params[MAX_PARAMS][64];
    int param_count;
    struct { char name[64]; Value value; } captured[MAX_CAPTURED];
    int captured_count;
};

Closure *closure_create(void);
void closure_add_param(Closure *c, const char *name, int len);
void closure_capture_current_scope(Closure *c);
Value invoke_closure(Closure *c, Value *args, int argc);

#endif
