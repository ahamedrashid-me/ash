#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/compiler.h"
#include "vm/builtins.h"
#include "lexer/lexer.h"

VMFunction vm_functions[MAX_VM_FUNCS];
int vm_function_count = 0;

static Token current;
static Token previous;

static void advance_token(void) { previous = current; current = lexer_next_token(); }

static void expect(TokenType type, const char *message) {
    if (current.type == type) { advance_token(); return; }
    fprintf(stderr, "parse error line %d: %s\n", current.line, message);
    exit(1);
}

static Chunk *chunk;
static char local_names[MAX_VM_LOCALS][64];
static int local_count;
static int lambda_counter = 0;

static int resolve_local(const char *name, int len) {
    for (int i = 0; i < local_count; i++) {
        if ((int)strlen(local_names[i]) == len && strncmp(local_names[i], name, len) == 0) return i;
    }
    return -1;
}

static int declare_local(const char *name, int len) {
    if (local_count >= MAX_VM_LOCALS) { fprintf(stderr, "too many locals\n"); exit(1); }
    memcpy(local_names[local_count], name, len);
    local_names[local_count][len] = '\0';
    return local_count++;
}

static void emit(uint8_t byte) { chunk_write(chunk, byte); }
static void emit2(uint8_t a, uint8_t b) { emit(a); emit(b); }

static void emit_constant(VMValue v) {
    int idx = chunk_add_constant(chunk, v);
    emit2(OP_CONST, (uint8_t)idx);
}

static int emit_jump(uint8_t op) { emit(op); emit(0xff); emit(0xff); return chunk->count - 2; }

static void patch_jump(int offset) {
    int jump = chunk->count - offset - 2;
    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

static void emit_loop(int loop_start) {
    emit(OP_LOOP);
    int offset = chunk->count - loop_start + 2;
    emit((offset >> 8) & 0xff);
    emit(offset & 0xff);
}

static int find_function(const char *name, int len) {
    for (int i = 0; i < vm_function_count; i++) {
        if ((int)strlen(vm_functions[i].name) == len && strncmp(vm_functions[i].name, name, len) == 0) return i;
    }
    return -1;
}

static void expression(void);
static void block(void);

static void emit_call(Token id) {
    int builtin_id = vm_builtin_lookup(id.start, id.length);
    int fn_idx = (builtin_id == -1) ? find_function(id.start, id.length) : -1;
    int local_slot = (builtin_id == -1 && fn_idx == -1) ? resolve_local(id.start, id.length) : -1;

    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expression(); argc++;
        while (current.type == TOKEN_COMMA) { advance_token(); expression(); argc++; }
    }
    expect(TOKEN_RPAREN, "expected ')' after arguments");

    if (builtin_id != -1) {
        emit(OP_CALL_BUILTIN);
        emit((uint8_t)builtin_id);
        emit((uint8_t)argc);
    } else if (fn_idx != -1) {
        if (argc != vm_functions[fn_idx].arity) {
            fprintf(stderr, "function %.*s expected %d args, got %d\n", id.length, id.start, vm_functions[fn_idx].arity, argc);
            exit(1);
        }
        emit(OP_CALL);
        emit((uint8_t)fn_idx);
        emit((uint8_t)argc);
    } else if (local_slot != -1) {
        emit2(OP_GET_LOCAL, (uint8_t)local_slot);
        emit(OP_CALL_VALUE);
        emit((uint8_t)argc);
    } else {
        fprintf(stderr, "undefined function: %.*s\n", id.length, id.start);
        exit(1);
    }
}

// fn(params) { body } as an expression. Captures the ENTIRE enclosing scope's
// locals by value at creation time (same rule as ashc's closures). The captured
// variables get their own reserved local slots in the lambda's own chunk, placed
// right after its parameters, so the compiled body can reference them via the
// normal resolve_local() mechanism just like any other local.
static void lambda_literal(void) {
    advance_token();
    expect(TOKEN_LPAREN, "expected '(' after 'fn'");

    char namebuf[32];
    int namelen = snprintf(namebuf, sizeof(namebuf), "__lambda_%d", lambda_counter++);
    if (namelen > (int)sizeof(namebuf) - 1) namelen = (int)sizeof(namebuf) - 1;

    if (vm_function_count >= MAX_VM_FUNCS) { fprintf(stderr, "too many functions\n"); exit(1); }
    int fn_idx = vm_function_count++;
    memcpy(vm_functions[fn_idx].name, namebuf, namelen);
    vm_functions[fn_idx].name[namelen] = '\0';
    vm_functions[fn_idx].arity = 0;
    chunk_init(&vm_functions[fn_idx].chunk);

    char param_names[8][64];
    int param_lens[8];
    int argc = 0;
    if (current.type != TOKEN_RPAREN) {
        expect(TOKEN_IDENTIFIER, "expected parameter name");
        memcpy(param_names[argc], previous.start, previous.length);
        param_lens[argc] = previous.length;
        argc++;
        while (current.type == TOKEN_COMMA) {
            advance_token();
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            memcpy(param_names[argc], previous.start, previous.length);
            param_lens[argc] = previous.length;
            argc++;
        }
    }
    expect(TOKEN_RPAREN, "expected ')' after parameters");
    vm_functions[fn_idx].arity = argc;

    int capture_count = local_count;
    char captured_names[MAX_VM_LOCALS][64];
    memcpy(captured_names, local_names, sizeof(local_names));

    Chunk *outer_chunk = chunk;
    char outer_locals[MAX_VM_LOCALS][64];
    int outer_local_count = local_count;
    memcpy(outer_locals, local_names, sizeof(local_names));

    chunk = &vm_functions[fn_idx].chunk;
    local_count = 0;
    for (int i = 0; i < argc; i++) declare_local(param_names[i], param_lens[i]);
    for (int i = 0; i < capture_count; i++) declare_local(captured_names[i], (int)strlen(captured_names[i]));

    block();
    emit_constant(vm_num(0));
    emit(OP_RETURN);

    chunk = outer_chunk;
    local_count = outer_local_count;
    memcpy(local_names, outer_locals, sizeof(local_names));

    emit(OP_MAKE_CLOSURE);
    emit((uint8_t)fn_idx);
    emit((uint8_t)capture_count);
}

static void primary(void) {
    if (current.type == TOKEN_FN) { lambda_literal(); return; }
    if (current.type == TOKEN_NUMBER) {
        int64_t v = strtoll(current.start, NULL, 10);
        advance_token();
        emit_constant(vm_num(v));
        return;
    }
    if (current.type == TOKEN_STRING) {
        char *s = vm_copy_string_escaped(current.start, current.length);
        advance_token();
        emit_constant(vm_str(s));
        return;
    }
    if (current.type == TOKEN_LBRACKET) {
        advance_token();
        int count = 0;
        if (current.type != TOKEN_RBRACKET) {
            expression(); count++;
            while (current.type == TOKEN_COMMA) { advance_token(); expression(); count++; }
        }
        expect(TOKEN_RBRACKET, "expected ']' after array literal");
        emit(OP_ARRAY);
        emit((uint8_t)count);
        return;
    }
    if (current.type == TOKEN_LBRACE) {
        advance_token();
        int n = 0;
        if (current.type != TOKEN_RBRACE) {
            for (;;) {
                if (current.type != TOKEN_STRING) { fprintf(stderr, "parse error line %d: expected string key in map literal\n", current.line); exit(1); }
                char *key = vm_copy_string_escaped(current.start, current.length);
                advance_token();
                expect(TOKEN_COLON, "expected ':' after map key");
                emit_constant(vm_str(key));
                expression();
                n++;
                if (current.type == TOKEN_COMMA) { advance_token(); continue; }
                break;
            }
        }
        expect(TOKEN_RBRACE, "expected '}' after map literal");
        emit(OP_MAP);
        emit((uint8_t)n);
        return;
    }
    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();
        if (current.type == TOKEN_LPAREN) {
            advance_token();
            emit_call(id);
            return;
        }
        int slot = resolve_local(id.start, id.length);
        if (slot != -1) { emit2(OP_GET_LOCAL, (uint8_t)slot); return; }
        int fn_idx = find_function(id.start, id.length);
        if (fn_idx != -1) { emit_constant(vm_function_val(fn_idx)); return; }
        fprintf(stderr, "undefined variable: %.*s\n", id.length, id.start);
        exit(1);
    }
    if (current.type == TOKEN_LPAREN) {
        advance_token();
        expression();
        expect(TOKEN_RPAREN, "expected ')' after expression");
        return;
    }
    fprintf(stderr, "parse error line %d: expected value\n", current.line);
    exit(1);
}

static void postfix(void) {
    primary();
    while (current.type == TOKEN_LBRACKET) {
        advance_token();
        expression();
        expect(TOKEN_RBRACKET, "expected ']' after index");
        emit(OP_INDEX_GET);
    }
}

static void unary(void) {
    if (current.type == TOKEN_BANG) { advance_token(); unary(); emit(OP_NOT); return; }
    if (current.type == TOKEN_MINUS) { advance_token(); unary(); emit(OP_NEG); return; }
    postfix();
}

static void term(void) {
    unary();
    while (current.type == TOKEN_STAR || current.type == TOKEN_SLASH || current.type == TOKEN_PERCENT) {
        TokenType op = current.type;
        advance_token();
        unary();
        emit(op == TOKEN_STAR ? OP_MUL : op == TOKEN_SLASH ? OP_DIV : OP_MOD);
    }
}

static void additive(void) {
    term();
    while (current.type == TOKEN_PLUS || current.type == TOKEN_MINUS) {
        TokenType op = current.type;
        advance_token();
        term();
        emit(op == TOKEN_PLUS ? OP_ADD : OP_SUB);
    }
}

static void comparison(void) {
    additive();
    if (current.type == TOKEN_EQUAL_EQUAL || current.type == TOKEN_BANG_EQUAL ||
        current.type == TOKEN_LESS || current.type == TOKEN_LESS_EQUAL ||
        current.type == TOKEN_GREATER || current.type == TOKEN_GREATER_EQUAL) {
        TokenType op = current.type;
        advance_token();
        additive();
        switch (op) {
            case TOKEN_EQUAL_EQUAL: emit(OP_EQ); break;
            case TOKEN_BANG_EQUAL: emit(OP_NEQ); break;
            case TOKEN_LESS: emit(OP_LT); break;
            case TOKEN_LESS_EQUAL: emit(OP_LE); break;
            case TOKEN_GREATER: emit(OP_GT); break;
            case TOKEN_GREATER_EQUAL: emit(OP_GE); break;
            default: break;
        }
    }
}

static void logical_and(void) {
    comparison();
    while (current.type == TOKEN_AND) { advance_token(); comparison(); emit(OP_AND); }
}
static void logical_or(void) {
    logical_and();
    while (current.type == TOKEN_OR) { advance_token(); logical_and(); emit(OP_OR); }
}
static void expression(void) { logical_or(); }

static int try_fuse_condition(void) {
    if (current.type != TOKEN_IDENTIFIER) return -1;
    Token save = current;
    Token id = current;
    advance_token();

    int cmp_code;
    switch (current.type) {
        case TOKEN_LESS: cmp_code = 0; break;
        case TOKEN_LESS_EQUAL: cmp_code = 1; break;
        case TOKEN_GREATER: cmp_code = 2; break;
        case TOKEN_GREATER_EQUAL: cmp_code = 3; break;
        case TOKEN_EQUAL_EQUAL: cmp_code = 4; break;
        case TOKEN_BANG_EQUAL: cmp_code = 5; break;
        default:
            lexer_init(save.start + save.length); current = save; return -1;
    }
    advance_token();

    int slot_a = resolve_local(id.start, id.length);
    if (slot_a == -1) { lexer_init(save.start + save.length); current = save; return -1; }

    uint8_t flags = (uint8_t)cmp_code;
    uint8_t b_operand;
    if (current.type == TOKEN_NUMBER) {
        int64_t v = strtoll(current.start, NULL, 10);
        b_operand = (uint8_t)chunk_add_constant(chunk, vm_num(v));
        flags |= 0x80;
        advance_token();
    } else if (current.type == TOKEN_IDENTIFIER) {
        int slot_b = resolve_local(current.start, current.length);
        if (slot_b == -1) { lexer_init(save.start + save.length); current = save; return -1; }
        b_operand = (uint8_t)slot_b;
        advance_token();
    } else {
        lexer_init(save.start + save.length); current = save; return -1;
    }

    if (current.type != TOKEN_RPAREN) {
        lexer_init(save.start + save.length); current = save; return -1;
    }

    emit(OP_CMP_JUMP);
    emit((uint8_t)slot_a);
    emit(flags);
    emit(b_operand);
    int patch_loc = chunk->count;
    emit(0xff); emit(0xff);
    return patch_loc;
}

static void statement(void);

static void block(void) {
    expect(TOKEN_LBRACE, "expected '{'");
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) statement();
    expect(TOKEN_RBRACE, "expected '}'");
}

static void statement(void) {
    if (current.type == TOKEN_PRINT) {
        advance_token();
        expression();
        expect(TOKEN_SEMICOLON, "expected ';' after statement");
        emit(OP_PRINT);
        return;
    }

    if (current.type == TOKEN_LET) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected variable name after 'let'");
        const char *name = previous.start;
        int len = previous.length;
        expect(TOKEN_EQUAL, "expected '=' after variable name");

        int slot = resolve_local(name, len);

        int fused = 0;
        if (slot != -1 && current.type == TOKEN_IDENTIFIER &&
            current.length == len && strncmp(current.start, name, len) == 0) {
            Token save = current;
            advance_token();
            int op_code = -1;
            if (current.type == TOKEN_PLUS) op_code = 0;
            else if (current.type == TOKEN_MINUS) op_code = 1;
            else if (current.type == TOKEN_STAR) op_code = 2;

            if (op_code != -1) {
                advance_token();
                uint8_t flags = (uint8_t)op_code;
                uint8_t b_operand;
                int have_operand = 0;
                if (current.type == TOKEN_NUMBER) {
                    int64_t v = strtoll(current.start, NULL, 10);
                    b_operand = (uint8_t)chunk_add_constant(chunk, vm_num(v));
                    flags |= 0x80;
                    advance_token();
                    have_operand = 1;
                } else if (current.type == TOKEN_IDENTIFIER) {
                    int src_slot = resolve_local(current.start, current.length);
                    if (src_slot != -1) {
                        b_operand = (uint8_t)src_slot;
                        advance_token();
                        have_operand = 1;
                    } else {
                        b_operand = 0;
                    }
                } else {
                    b_operand = 0;
                }

                if (have_operand && current.type == TOKEN_SEMICOLON) {
                    advance_token();
                    emit(OP_ACC_LOCAL);
                    emit((uint8_t)slot);
                    emit(flags);
                    emit(b_operand);
                    fused = 1;
                }
            }
            if (!fused) { lexer_init(save.start + save.length); current = save; }
        }

        if (!fused) {
            expression();
            expect(TOKEN_SEMICOLON, "expected ';' after statement");
            if (slot != -1) { emit2(OP_SET_LOCAL, (uint8_t)slot); emit(OP_POP); }
            else declare_local(name, len);
        }
        return;
    }

    if (current.type == TOKEN_RETURN) {
        advance_token();
        if (current.type != TOKEN_SEMICOLON) expression();
        else emit_constant(vm_num(0));
        expect(TOKEN_SEMICOLON, "expected ';' after return");
        emit(OP_RETURN);
        return;
    }

    if (current.type == TOKEN_FN) {
        advance_token();
        expect(TOKEN_IDENTIFIER, "expected function name after 'fn'");
        const char *name = previous.start;
        int name_len = previous.length;
        expect(TOKEN_LPAREN, "expected '(' after function name");

        if (vm_function_count >= MAX_VM_FUNCS) { fprintf(stderr, "too many functions\n"); exit(1); }
        int fn_idx = vm_function_count++;
        strncpy(vm_functions[fn_idx].name, name, name_len);
        vm_functions[fn_idx].name[name_len] = '\0';
        vm_functions[fn_idx].arity = 0;
        chunk_init(&vm_functions[fn_idx].chunk);

        char param_names[8][64];
        int param_lens[8];
        int argc = 0;
        if (current.type != TOKEN_RPAREN) {
            expect(TOKEN_IDENTIFIER, "expected parameter name");
            memcpy(param_names[argc], previous.start, previous.length);
            param_lens[argc] = previous.length;
            argc++;
            while (current.type == TOKEN_COMMA) {
                advance_token();
                expect(TOKEN_IDENTIFIER, "expected parameter name");
                memcpy(param_names[argc], previous.start, previous.length);
                param_lens[argc] = previous.length;
                argc++;
            }
        }
        expect(TOKEN_RPAREN, "expected ')' after parameters");
        vm_functions[fn_idx].arity = argc;

        Chunk *outer_chunk = chunk;
        char outer_locals[MAX_VM_LOCALS][64];
        int outer_local_count = local_count;
        memcpy(outer_locals, local_names, sizeof(local_names));

        chunk = &vm_functions[fn_idx].chunk;
        local_count = 0;
        for (int i = 0; i < argc; i++) declare_local(param_names[i], param_lens[i]);

        block();
        emit_constant(vm_num(0));
        emit(OP_RETURN);

        chunk = outer_chunk;
        local_count = outer_local_count;
        memcpy(local_names, outer_locals, sizeof(local_names));
        return;
    }

    if (current.type == TOKEN_IF) {
        advance_token();
        expect(TOKEN_LPAREN, "expected '(' after 'if'");
        int patch_loc = try_fuse_condition();
        int else_jump;
        if (patch_loc != -1) {
            expect(TOKEN_RPAREN, "expected ')' after condition");
            else_jump = patch_loc;
        } else {
            expression();
            expect(TOKEN_RPAREN, "expected ')' after condition");
            else_jump = emit_jump(OP_JUMP_IF_FALSE);
        }
        block();
        if (current.type == TOKEN_ELSE) {
            int end_jump = emit_jump(OP_JUMP);
            patch_jump(else_jump);
            advance_token();
            block();
            patch_jump(end_jump);
        } else {
            patch_jump(else_jump);
        }
        return;
    }

    if (current.type == TOKEN_TRY) {
        advance_token();
        int saved_local_count = local_count;

        int try_jump = emit_jump(OP_TRY_PUSH);

        block();

        emit(OP_TRY_POP);
        int skip_catch = emit_jump(OP_JUMP);

        patch_jump(try_jump);

        expect(TOKEN_CATCH, "expected 'catch' after try block");
        expect(TOKEN_LPAREN, "expected '(' after 'catch'");
        expect(TOKEN_IDENTIFIER, "expected error variable name");
        // 'e' must land at EXACTLY saved_local_count, not wherever local_count
        // ended up after compiling the try block -- since the try block may have
        // declared its own locals before erroring, but the runtime error-value
        // push always lands at the try-entry stack position, regardless of what
        // the try block attempted (all of that gets discarded on the error path).
        memcpy(local_names[saved_local_count], previous.start, previous.length);
        local_names[saved_local_count][previous.length] = '\0';
        local_count = saved_local_count + 1;
        expect(TOKEN_RPAREN, "expected ')' after catch variable");

        block();
        emit(OP_POP);

        patch_jump(skip_catch);

        local_count = saved_local_count;
        return;
    }

    if (current.type == TOKEN_WHILE) {
        advance_token();
        expect(TOKEN_LPAREN, "expected '(' after 'while'");
        int loop_start = chunk->count;
        int patch_loc = try_fuse_condition();
        int exit_jump;
        if (patch_loc != -1) {
            expect(TOKEN_RPAREN, "expected ')' after condition");
            exit_jump = patch_loc;
        } else {
            expression();
            expect(TOKEN_RPAREN, "expected ')' after condition");
            exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        }
        block();
        emit_loop(loop_start);
        patch_jump(exit_jump);
        return;
    }

    if (current.type == TOKEN_IDENTIFIER) {
        Token id = current;
        advance_token();
        if (current.type == TOKEN_LPAREN) {
            advance_token();
            emit_call(id);
            expect(TOKEN_SEMICOLON, "expected ';' after call");
            emit(OP_POP);
            return;
        }
        if (current.type == TOKEN_LBRACKET) {
            int slot = resolve_local(id.start, id.length);
            if (slot == -1) { fprintf(stderr, "undefined variable: %.*s\n", id.length, id.start); exit(1); }
            emit2(OP_GET_LOCAL, (uint8_t)slot);
            advance_token();
            expression();
            expect(TOKEN_RBRACKET, "expected ']'");
            expect(TOKEN_EQUAL, "expected '=' for index assignment");
            expression();
            expect(TOKEN_SEMICOLON, "expected ';' after assignment");
            emit(OP_INDEX_SET);
            return;
        }
        fprintf(stderr, "parse error line %d: unexpected identifier statement\n", current.line);
        exit(1);
    }

    fprintf(stderr, "parse error line %d: unexpected token\n", current.line);
    exit(1);
}

static Chunk main_chunk;

Chunk *compile(const char *source) {
    vm_function_count = 0;
    local_count = 0;
    chunk_init(&main_chunk);
    chunk = &main_chunk;
    lexer_init(source);
    advance_token();
    while (current.type != TOKEN_EOF) statement();
    return &main_chunk;
}

// Appends compiled bytecode to an EXISTING chunk without resetting locals or
// function definitions -- used by the REPL so state persists between lines.
Chunk *compile_repl_line(const char *source, Chunk *target_chunk, int *out_start_offset) {
    chunk = target_chunk;
    *out_start_offset = chunk->count;
    lexer_init(source);
    advance_token();
    while (current.type != TOKEN_EOF) statement();
    return chunk;
}
