#ifndef ASH_BUILTINS_H
#define ASH_BUILTINS_H

#include "value/value.h"

int is_builtin(const char *name, int len);
Value call_builtin(const char *name, int len, Value *args, int argc);

#endif
