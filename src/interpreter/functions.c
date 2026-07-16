#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter/functions.h"
#include "interpreter/parser_state.h"
#include "interpreter/statements.h"
#include "env/env.h"
#include "lexer/lexer.h"
#include "interpreter/error.h"

static Function functions[MAX_FUNCTIONS];
static int function_count = 0;

void functions_reset(void) {
    function_count = 0;
}

Function *function_register(const char *name, int name_len) {
    if (function_count >= MAX_FUNCTIONS) {
        runtime_error("too many functions");
    }
    Function *fn = &functions[function_count++];
    strncpy(fn->name, name, name_len);
    fn->name[name_len] = '\0';
    fn->param_count = 0;
    return fn;
}

void function_add_param(Function *fn, const char *name, int len) {
    if (fn->param_count >= MAX_PARAMS) {
        runtime_error("too many parameters");
    }
    strncpy(fn->params[fn->param_count], name, len);
    fn->params[fn->param_count][len] = '\0';
    fn->param_count++;
}

int function_find_index(const char *name, int len) {
    for (int i = 0; i < function_count; i++) {
        if ((int)strlen(functions[i].name) == len && strncmp(functions[i].name, name, len) == 0) {
            return i;
        }
    }
    return -1;
}

int function_exists(const char *name, int len) {
    return function_find_index(name, len) != -1;
}

int function_last_index(void) {
    return function_count - 1;
}

// Single choke point where we jump into a function body's tokens. ANY caller
// (a direct call, or a nested call from inside a builtin like map/filter/reduce)
// must have its parsing position saved before we jump, and restored after —
// otherwise the caller's `current` token gets left wherever the callee's body
// happens to sit in the source file, corrupting whatever the caller was doing.
static Value call_by_index(int idx, Value *args, int argc) {
    Function *fn = &functions[idx];
    if (argc != fn->param_count) {
        runtime_error("function %s expected %d args, got %d", fn->name, fn->param_count, argc);
    }

    env_push_scope();
    for (int i = 0; i < argc; i++) {
        set_variable(fn->params[i], (int)strlen(fn->params[i]), args[i]);
    }

    Token caller_resume = current;

    lexer_init(fn->body_start);
    advance_token();

    return_flag = 0;
    block();

    env_pop_scope();

    Value result = return_flag ? return_value : num_val(0);
    return_flag = 0;

    lexer_init(caller_resume.start + caller_resume.length);
    current = caller_resume;

    return result;
}

Value invoke_function(const char *name, int len, Value *args, int argc) {
    int idx = function_find_index(name, len);
    if (idx == -1) {
        runtime_error("undefined function: %.*s", len, name);
    }
    return call_by_index(idx, args, argc);
}

Value invoke_function_by_index(int idx, Value *args, int argc) {
    return call_by_index(idx, args, argc);
}
