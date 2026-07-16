#ifndef ASH_PARSER_STATE_H
#define ASH_PARSER_STATE_H

#include "lexer/lexer.h"

extern Token current;
extern Token previous;

void advance_token(void);
void expect(TokenType type, const char *message);
void parser_init(const char *source);

#endif
