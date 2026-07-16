#ifndef ASH_STATEMENTS_H
#define ASH_STATEMENTS_H

#include "value/value.h"

extern int return_flag;
extern Value return_value;

void block(void);
void interpret(const char *source);
void interpret_repl_line(const char *source);

#endif
