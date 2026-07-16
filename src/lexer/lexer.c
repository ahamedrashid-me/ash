#include <string.h>
#include <ctype.h>
#include "lexer/lexer.h"

static const char *start;
static const char *current;
static int line;

void lexer_init(const char *source) {
    start = source;
    current = source;
    line = 1;
}

static int is_at_end(void) { return *current == '\0'; }
static char advance(void) { current++; return current[-1]; }
static char peek(void) { return *current; }
static char peek_next(void) { if (is_at_end()) return '\0'; return current[1]; }

static int match(char expected) {
    if (is_at_end()) return 0;
    if (*current != expected) return 0;
    current++;
    return 1;
}

static void skip_whitespace(void) {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t':
                advance();
                break;
            case '\n':
                line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = start;
    token.length = (int)(current - start);
    token.line = line;
    return token;
}

static Token error_token(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = line;
    return token;
}

static Token number(void) {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance(); // consume '.'
        while (isdigit(peek())) advance();
    }
    return make_token(TOKEN_NUMBER);
}

static Token string_token(void) {
    const char *content_start = current;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\\' && peek_next() != '\0') {
            advance(); // skip the backslash
            advance(); // skip the escaped character itself, so a \" doesn't end the string early
            continue;
        }
        if (peek() == '\n') line++;
        advance();
    }
    if (is_at_end()) return error_token("unterminated string");

    int length = (int)(current - content_start);
    advance();

    Token token;
    token.type = TOKEN_STRING;
    token.start = content_start;
    token.length = length;
    token.line = line;
    return token;
}

static Token identifier(void) {
    while (isalnum(peek()) || peek() == '_') advance();

    int length = (int)(current - start);
    if (length == 5 && strncmp(start, "print", 5) == 0) return make_token(TOKEN_PRINT);
    if (length == 3 && strncmp(start, "let", 3) == 0) return make_token(TOKEN_LET);
    if (length == 2 && strncmp(start, "if", 2) == 0) return make_token(TOKEN_IF);
    if (length == 4 && strncmp(start, "else", 4) == 0) return make_token(TOKEN_ELSE);
    if (length == 5 && strncmp(start, "while", 5) == 0) return make_token(TOKEN_WHILE);
    if (length == 3 && strncmp(start, "for", 3) == 0) return make_token(TOKEN_FOR);
    if (length == 2 && strncmp(start, "in", 2) == 0) return make_token(TOKEN_IN);
    if (length == 2 && strncmp(start, "fn", 2) == 0) return make_token(TOKEN_FN);
    if (length == 6 && strncmp(start, "return", 6) == 0) return make_token(TOKEN_RETURN);
    if (length == 3 && strncmp(start, "try", 3) == 0) return make_token(TOKEN_TRY);
    if (length == 5 && strncmp(start, "catch", 5) == 0) return make_token(TOKEN_CATCH);

    return make_token(TOKEN_IDENTIFIER);
}

Token lexer_next_token(void) {
    skip_whitespace();
    start = current;

    if (is_at_end()) return make_token(TOKEN_EOF);

    char c = advance();

    if (isdigit(c)) return number();
    if (isalpha(c) || c == '_') return identifier();
    if (c == '"') return string_token();

    switch (c) {
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case ':': return make_token(TOKEN_COLON);
        case '+': return make_token(TOKEN_PLUS);
        case '-': return make_token(TOKEN_MINUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '%': return make_token(TOKEN_PERCENT);
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '&':
            if (match('&')) return make_token(TOKEN_AND);
            return error_token("unexpected character");
        case '|':
            if (match('|')) return make_token(TOKEN_OR);
            return error_token("unexpected character");
    }

    return error_token("unexpected character");
}
