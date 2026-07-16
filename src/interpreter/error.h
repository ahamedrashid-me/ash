#ifndef ASH_ERROR_H
#define ASH_ERROR_H
#include <setjmp.h>

void error_init(void);
void error_set_source(const char *source, const char *filename);
jmp_buf *error_push_handler(void);
void error_pop_handler(void);
const char *error_last_message(void);

void runtime_error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void parse_error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

#endif
