#ifndef ASH_FUNCTIONS_H
#define ASH_FUNCTIONS_H

#include "value/value.h"
#include "common.h"

typedef struct {
    char name[64];
    char params[MAX_PARAMS][64];
    int param_count;
    const char *body_start;
} Function;

void functions_reset(void);
Function *function_register(const char *name, int name_len);
void function_add_param(Function *fn, const char *name, int len);
int function_exists(const char *name, int len);
int function_find_index(const char *name, int len);
int function_last_index(void);
Value invoke_function(const char *name, int len, Value *args, int argc);
Value invoke_function_by_index(int idx, Value *args, int argc);

#endif
