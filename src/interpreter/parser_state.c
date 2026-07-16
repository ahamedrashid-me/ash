#include <stdio.h>
#include <stdlib.h>
#include "interpreter/parser_state.h"
#include "interpreter/error.h"

Token current;
Token previous;

void advance_token(void) {
    previous = current;
    current = lexer_next_token();
}

void expect(TokenType type, const char *message) {
    if (current.type == type) {
        advance_token();
        return;
    }
    parse_error("%s", message);
}

void parser_init(const char *source) {
    lexer_init(source);
    advance_token();
}
