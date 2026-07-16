#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "interpreter/expressions.h"
#include "interpreter/parser_state.h"
#include "interpreter/functions.h"
#include "interpreter/closures.h"
#include "builtins/builtins.h"
#include "env/env.h"
#include "lexer/lexer.h"
#include "value/hashmap.h"
#include "common.h"
#include "interpreter/error.h"

static Value expression_inner(void);

static Value do_call(Token id) {
    Value args[MAX_CALL_ARGS];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        args[argc++] = expression_inner();
        while (current.type == TOKEN_COMMA) {
            advance_token();
            if (argc >= MAX_CALL_ARGS) { runtime_error("too many arguments"); exit(1); }
            args[argc++] = expression_inner();
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    if (is_builtin(id.start, id.length)) return call_builtin(id.start, id.length, args, argc);
    if (function_exists(id.start, id.length)) return invoke_function(id.start, id.length, args, argc);

    Value fnval = get_variable(id.start, id.length);
    if (fnval.type == VAL_FUNCTION) return invoke_function_by_index(fnval.function_index, args, argc);
    if (fnval.type == VAL_CLOSURE) return invoke_closure(fnval.closure, args, argc);
    runtime_error("%.*s is not callable", id.length, id.start);
    return num_val(0); // unreachable
}

static Value lambda_literal(void) {
    advance_token();
    expect(TOKEN_LPAREN, "expected '(' after 'fn'");

    Closure *c = closure_create();
    if (current.type != TOKEN_RPAREN) {
        expect(TOKEN_IDENTIFIER, "expected parameter name");
        closure_add_param(c, previous.start, previous.length);
        while (current.type == TOKEN_COMMA) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            closure_add_param(c, previous.start, previous.length);
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");
    c->body_start = current.start;

    closure_capture_current_scope(c);

    expect(TOKEN_LBRACE, "expected '{'");
    int depth = 1;
    while (depth > 0) {
        if (current.type == TOKEN_LBRACE) depth++;
        if (current.type == TOKEN_RBRACE) depth--;
        if (current.type == TOKEN_EOF) { runtime_error("unterminated lambda body"); exit(1); }
        if (depth > 0) advance_token();
    }
    advance_token();

    return closure_val(c);
}

static Value map_literal(void) {
    advance_token(); // consume '{'
    HashMap *m = map_new();
    if (current.type != TOKEN_RBRACE) {
        for (;;) {
            expect(TOKEN_STRING, "expected string key in map literal");
            char *key = copy_string_escaped(previous.start, previous.length);
            expect(TOKEN_COLON, "expected ':' after map key");
            Value val = expression_inner();
            map_set(m, key, val);
            if (current.type == TOKEN_COMMA) { advance_token(); continue; }
            break;
        }
    }
    expect(TOKEN_RBRACE, "expected '}' after map literal");
    return map_val(m);
}

static Value primary(void) {
    if (current.type == TOKEN_FN) return lambda_literal();
    if (current.type == TOKEN_LBRACE) return map_literal();
    if (current.type == TOKEN_NUMBER) {
        advance_token();
        return num_val(strtod(previous.start, NULL));
    }
    if (current.type == TOKEN_STRING) {
        advance_token();
        return str_val(copy_string_escaped(previous.start, previous.length));
    }
    if (current.type == TOKEN_LBRACKET) {
        advance_token();
        ValueArray *arr = array_new();
        if (current.type != TOKEN_RBRACKET) {
            array_push(arr, expression_inner());
            while (current.type == TOKEN_COMMA) {
                advance_token();
                array_push(arr, expression_inner());
            }
        }
        expect(TOKEN_RBRACKET, "expected ']' after array literal");
        return array_val(arr);
    }
    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();
        if (current.type == TOKEN_LPAREN) {
            advance_token();
            return do_call(id);
        }
        Value v;
        if (try_get_variable(id.start, id.length, &v)) return v;
        if (function_exists(id.start, id.length)) return func_val(function_find_index(id.start, id.length));
        runtime_error("undefined variable: %.*s", id.length, id.start);
    }
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        Value value = expression_inner();
        expect(TOKEN_RPAREN, "expected ')' after expression");
        return value;
    }
    parse_error("expected value");
}

static Value postfix(void) {
    Value value = primary();
    while (current.type == TOKEN_LBRACKET) {
        advance_token();
        Value idx = expression_inner();
        expect(TOKEN_RBRACKET, "expected ']' after index");
        if (value.type == VAL_ARRAY) {
            int i = (int)require_number(idx, "array index");
            if (i < 0 || i >= value.array->count) { runtime_error("index out of bounds: %d", i); exit(1); }
            value = value.array->items[i];
        } else if (value.type == VAL_MAP) {
            if (idx.type != VAL_STRING) { runtime_error("map keys must be strings"); }
            Value out;
            if (!map_get(value.map, idx.str, &out)) { runtime_error("key not found: %s", idx.str); }
            value = out;
        } else {
            runtime_error("type error: cannot index this value");
        }
    }
    return value;
}

static Value unary(void) {
    if (current.type == TOKEN_BANG) {
        advance_token();
        Value v = unary();
        return num_val(require_number(v, "'!'") == 0.0 ? 1 : 0);
    }
    if (current.type == TOKEN_MINUS) {
        advance_token();
        Value v = unary();
        return num_val(-require_number(v, "unary '-'"));
    }
    return postfix();
}

static Value term(void) {
    Value value = unary();
    while (current.type == TOKEN_STAR || current.type == TOKEN_SLASH || current.type == TOKEN_PERCENT) {
        TokenType op = current.type;
        advance_token();
        Value rhs = unary();
        double a = require_number(value, "'*'/'/'/'%'");
        double b = require_number(rhs, "'*'/'/'/'%'");
        if (op == TOKEN_STAR) value = num_val(a * b);
        else if (op == TOKEN_SLASH) value = num_val(a / b);
        else value = num_val(fmod(a, b));
    }
    return value;
}

static Value additive(void) {
    Value value = term();
    while (current.type == TOKEN_PLUS || current.type == TOKEN_MINUS) {
        TokenType op = current.type;
        advance_token();
        Value rhs = term();
        if (op == TOKEN_PLUS) {
            if (value.type == VAL_STRING && rhs.type == VAL_STRING) value = str_val(concat_strings(value.str, rhs.str));
            else value = num_val(require_number(value, "'+'") + require_number(rhs, "'+'"));
        } else {
            value = num_val(require_number(value, "'-'") - require_number(rhs, "'-'"));
        }
    }
    return value;
}

static Value comparison(void) {
    Value value = additive();
    if (current.type == TOKEN_EQUAL_EQUAL || current.type == TOKEN_BANG_EQUAL ||
        current.type == TOKEN_LESS || current.type == TOKEN_LESS_EQUAL ||
        current.type == TOKEN_GREATER || current.type == TOKEN_GREATER_EQUAL) {
        TokenType op = current.type;
        advance_token();
        Value rhs = additive();
        if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) {
            int equal;
            if (value.type == VAL_STRING && rhs.type == VAL_STRING) equal = strcmp(value.str, rhs.str) == 0;
            else if (value.type == VAL_NUMBER && rhs.type == VAL_NUMBER) equal = value.number == rhs.number;
            else equal = 0;
            return num_val(op == TOKEN_EQUAL_EQUAL ? equal : !equal);
        }
        double a = require_number(value, "comparison");
        double b = require_number(rhs, "comparison");
        switch (op) {
            case TOKEN_LESS: return num_val(a < b);
            case TOKEN_LESS_EQUAL: return num_val(a <= b);
            case TOKEN_GREATER: return num_val(a > b);
            case TOKEN_GREATER_EQUAL: return num_val(a >= b);
            default: break;
        }
    }
    return value;
}

static Value logical_and(void) {
    Value value = comparison();
    while (current.type == TOKEN_AND) {
        advance_token();
        Value rhs = comparison();
        value = num_val((require_number(value, "'&&'") != 0.0 && require_number(rhs, "'&&'") != 0.0) ? 1 : 0);
    }
    return value;
}

static Value logical_or(void) {
    Value value = logical_and();
    while (current.type == TOKEN_OR) {
        advance_token();
        Value rhs = logical_and();
        value = num_val((require_number(value, "'||'") != 0.0 || require_number(rhs, "'||'") != 0.0) ? 1 : 0);
    }
    return value;
}

static Value expression_inner(void) { return logical_or(); }
Value expression(void) { return expression_inner(); }
