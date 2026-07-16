#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter/statements.h"
#include "interpreter/parser_state.h"
#include "interpreter/expressions.h"
#include "interpreter/functions.h"
#include "interpreter/error.h"
#include "env/env.h"
#include "lexer/lexer.h"
#include "value/value.h"
#include "value/hashmap.h"

int return_flag = 0;
Value return_value;

static void statement(void);

void block(void) {
    expect(TOKEN_LBRACE, "expected '{'");
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF && !return_flag) {
        statement();
    }
    if (return_flag) {
        int depth = 1;
        while (depth > 0) {
            if (current.type == TOKEN_LBRACE) depth++;
            if (current.type == TOKEN_RBRACE) depth--;
            if (current.type == TOKEN_EOF) {
                fprintf(stderr, "unterminated block\n");
                exit(1);
            }
            if (depth > 0) advance_token();
        }
        advance_token();
        return;
    }
    expect(TOKEN_RBRACE, "expected '}'");
}

static void skip_block(void) {
    expect(TOKEN_LBRACE, "expected '{'");
    int depth = 1;
    while (depth > 0) {
        if (current.type == TOKEN_LBRACE) depth++;
        if (current.type == TOKEN_RBRACE) depth--;
        if (current.type == TOKEN_EOF) {
            fprintf(stderr, "unterminated block\n");
            exit(1);
        }
        if (depth > 0) advance_token();
    }
    advance_token();
}

// Scans raw source characters (NOT tokens - doesn't touch parser state) starting at a
// '{' to find the matching '}', skipping over any braces inside string literals.
// Needed because if an error longjmps out mid-way through executing a try block, we
// won't have naturally parsed our way to its closing brace, so we need to know in
// advance exactly where to resume once we jump into the catch clause.
static const char *scan_matching_brace(const char *open_brace) {
    const char *p = open_brace;
    int depth = 0;
    while (*p) {
        char c = *p;
        if (c == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1] != '\0') p++;
                p++;
            }
            if (*p == '"') p++;
            continue;
        }
        if (c == '{') depth++;
        if (c == '}') {
            depth--;
            if (depth == 0) return p + 1;
        }
        p++;
    }
    fprintf(stderr, "unterminated block while scanning for try/catch\n");
    exit(1);
}

static void statement(void) {
    if (current.type == TOKEN_PRINT) {
        advance_token();
        Value value = expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        print_value(value);
        printf("\n");
        return;
    }

    if (current.type == TOKEN_LET) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected variable name after 'let'");
        const char *name = previous.start;
        int length = previous.length;
        expect(TOKEN_EQUAL, "expected '=' after variable name");
        Value value = expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        set_variable(name, length, value);
        return;
    }

    if (current.type == TOKEN_RETURN) {
        advance_token();
        if (current.type != TOKEN_SEMICOLON) {
            return_value = expression();
        } else {
            return_value = num_val(0);
        }
        expect(TOKEN_SEMICOLON, "expected ';' after return");
        return_flag = 1;
        return;
    }

    if (current.type == TOKEN_TRY) {
        advance_token();

        if (current.type != TOKEN_LBRACE) {
            parse_error("expected '{' after 'try'");
        }
        const char *after_try_block = scan_matching_brace(current.start);

        int saved_depth = env_get_depth();
        jmp_buf *handler = error_push_handler();

        if (setjmp(*handler) == 0) {
            block();
            error_pop_handler();
        } else {
            error_pop_handler();
            env_set_depth(saved_depth);

            lexer_init(after_try_block);
            advance_token();

            expect(TOKEN_CATCH, "expected 'catch' after try block");
            expect(TOKEN_LPAREN, "expected '(' after 'catch'");
            expect(TOKEN_IDENTIFIER, "expected error variable name");
            const char *err_name = previous.start;
            int err_len = previous.length;
            expect(TOKEN_RPAREN, "expected ')' after catch variable");

            const char *msg = error_last_message();
            set_variable(err_name, err_len, str_val(copy_string(msg, (int)strlen(msg))));

            block();
            return;
        }

        // try block succeeded with no error; skip the (unexecuted) catch clause
        expect(TOKEN_CATCH, "expected 'catch' after try block");
        expect(TOKEN_LPAREN, "expected '(' after 'catch'");
        expect(TOKEN_IDENTIFIER, "expected error variable name");
        expect(TOKEN_RPAREN, "expected ')' after catch variable");
        skip_block();
        return;
    }

    if (current.type == TOKEN_FN) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected function name after 'fn'");
        const char *name = previous.start;
        int name_len = previous.length;
        expect(TOKEN_LPAREN, "expected '(' after function name");

        Function *fn = function_register(name, name_len);

        if (current.type != TOKEN_RPAREN) {
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            function_add_param(fn, previous.start, previous.length);
            while (current.type == TOKEN_COMMA) {
                advance_token();
                expect(TOKEN_IDENTIFIER, "expected parameter name");
                function_add_param(fn, previous.start, previous.length);
            }
        }
        expect(TOKEN_RPAREN, "expected ')' after parameters");

        fn->body_start = current.start;
        skip_block();
        return;
    }

    if (current.type == TOKEN_IF) {
        advance_token();
        expect(TOKEN_LPAREN, "expected '(' after 'if'");
        Value cond = expression();
        double condition = require_number(cond, "if condition");
        expect(TOKEN_RPAREN, "expected ')' after condition");

        if (condition != 0.0) {
            block();
            if (!return_flag && current.type == TOKEN_ELSE) {
                advance_token();
                skip_block();
            }
        } else {
            skip_block();
            if (current.type == TOKEN_ELSE) {
                advance_token();
                block();
            }
        }
        return;
    }

    if (current.type == TOKEN_WHILE) {
        advance_token();
        expect(TOKEN_LPAREN, "expected '(' after 'while'");
        const char *condition_start = current.start;
        Value cond = expression();
        double condition = require_number(cond, "while condition");
        expect(TOKEN_RPAREN, "expected ')' after condition");
        const char *body_start = current.start;

        while (condition != 0.0) {
            lexer_init(body_start);
            advance_token();
            block();
            if (return_flag) return;

            lexer_init(condition_start);
            advance_token();
            cond = expression();
            condition = require_number(cond, "while condition");
            expect(TOKEN_RPAREN, "expected ')' after condition");
        }

        lexer_init(body_start);
        advance_token();
        skip_block();
        return;
    }

    if (current.type == TOKEN_FOR) {
        advance_token();
        expect(TOKEN_LPAREN, "expected '(' after 'for'");
        expect(TOKEN_IDENTIFIER, "expected loop variable name");
        const char *var_name = previous.start;
        int var_len = previous.length;
        expect(TOKEN_IN, "expected 'in' after loop variable");

        Value array = expression();
        expect(TOKEN_RPAREN, "expected ')' after for-in expression");
        const char *body_start = current.start;

        if (array.type != VAL_ARRAY) {
            fprintf(stderr, "for-in expects an array\n");
            exit(1);
        }

        for (int i = 0; i < array.array->count; i++) {
            set_variable(var_name, var_len, array.array->items[i]);
            lexer_init(body_start);
            advance_token();
            block();
            if (return_flag) return;
        }

        lexer_init(body_start);
        advance_token();
        skip_block();
        return;
    }

    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();

        if (current.type == TOKEN_LBRACKET) {
            advance_token();
            Value idx = expression();
            expect(TOKEN_RBRACKET, "expected ']'");
            if (current.type == TOKEN_EQUAL) {
                advance_token();
                Value new_val = expression();
                expect(TOKEN_SEMICOLON, "expected ';' after assignment");
                Value target = get_variable(id.start, id.length);
                if (target.type == VAL_ARRAY) {
                    int i = (int)require_number(idx, "array index");
                    if (i < 0 || i >= target.array->count) {
                        runtime_error("index out of bounds: %d", i);
                    }
                    target.array->items[i] = new_val;
                } else if (target.type == VAL_MAP) {
                    if (idx.type != VAL_STRING) runtime_error("map keys must be strings");
                    map_set(target.map, idx.str, new_val);
                } else {
                    runtime_error("cannot index-assign this value");
                }
                return;
            }
            expect(TOKEN_SEMICOLON, "expected ';' after expression");
            return;
        }

        lexer_init(id.start + id.length);
        current = id;
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after expression");
        return;
    }

    parse_error("unexpected token");
}

void interpret(const char *source) {
    env_reset();
    functions_reset();
    error_init();
    return_flag = 0;

    parser_init(source);

    while (current.type != TOKEN_EOF) {
        statement();
    }
}

void interpret_repl_line(const char *source) {
    parser_init(source);

    while (current.type != TOKEN_EOF) {
        statement();
    }
}
