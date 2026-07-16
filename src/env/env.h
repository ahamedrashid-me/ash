#ifndef ASH_ENV_H
#define ASH_ENV_H

#include "value/value.h"

void env_reset(void);
void env_push_scope(void);
void env_pop_scope(void);

void set_variable(const char *name, int length, Value value);
Value get_variable(const char *name, int length);
int try_get_variable(const char *name, int length, Value *out);

int env_get_depth(void);
void env_set_depth(int depth);

int env_snapshot_current_scope(char names_out[][64], Value *values_out, int max);

#endif
