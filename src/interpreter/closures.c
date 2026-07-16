#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter/closures.h"
#include "interpreter/parser_state.h"
#include "interpreter/statements.h"
#include "env/env.h"
#include "lexer/lexer.h"
#include "interpreter/error.h"

Closure *closure_create(void) {
    Closure *c = malloc(sizeof(Closure));
    c->param_count = 0;
    c->captured_count = 0;
    return c;
}

void closure_add_param(Closure *c, const char *name, int len) {
    strncpy(c->params[c->param_count], name, len);
    c->params[c->param_count][len] = '\0';
    c->param_count++;
}

void closure_capture_current_scope(Closure *c) {
    char names[MAX_CAPTURED][64];
    Value values[MAX_CAPTURED];
    int n = env_snapshot_current_scope(names, values, MAX_CAPTURED);
    for (int i = 0; i < n; i++) {
        strcpy(c->captured[i].name, names[i]);
        c->captured[i].value = values[i];
    }
    c->captured_count = n;
}

// Same shared-choke-point pattern as call_by_index in functions.c: save the
// caller's parsing position before jumping into the closure body, restore after.
Value invoke_closure(Closure *c, Value *args, int argc) {
    if (argc != c->param_count) {
        runtime_error("closure expected %d args, got %d", c->param_count, argc);
    }

    env_push_scope();

    // populate captured (outer) variables first, so params correctly shadow them
    for (int i = 0; i < c->captured_count; i++) {
        set_variable(c->captured[i].name, (int)strlen(c->captured[i].name), c->captured[i].value);
    }
    for (int i = 0; i < argc; i++) {
        set_variable(c->params[i], (int)strlen(c->params[i]), args[i]);
    }

    Token caller_resume = current;

    lexer_init(c->body_start);
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
