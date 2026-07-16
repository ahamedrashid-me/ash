#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include "vm/vm.h"
#include "vm/compiler.h"
#include "vm/builtins.h"
#include "vm/hashmap.h"

#define STACK_MAX 65536
#define FRAMES_MAX 256
#define MAX_TRY_DEPTH 32

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    VMValue *slots;
} CallFrame;

static VMValue stack[STACK_MAX];
static VMValue *stack_top;
static CallFrame frames[FRAMES_MAX];
static int frame_count;

typedef struct {
    jmp_buf jmp;
    int saved_frame_count;
    VMValue *saved_stack_top;
    CallFrame *target_frame;
    uint8_t *target_ip;
} TryHandler;

static TryHandler try_handlers[MAX_TRY_DEPTH];
static int try_depth = 0;
static char error_message[512];

static jmp_buf *repl_recovery_jmp = NULL;
static int repl_saved_frame_count;
static VMValue *repl_saved_stack_top;

void vm_set_repl_recovery(jmp_buf *jb) {
    repl_recovery_jmp = jb;
}

void vm_repl_checkpoint(void) {
    repl_saved_frame_count = frame_count;
    repl_saved_stack_top = stack_top;
}

// Every genuinely-runtime error (as opposed to a compile-time/structural one,
// which stays a hard crash since ashvm resolves names at compile time) funnels
// through here. If a try block is active, longjmp back to its handler instead
// of crashing the whole program.
void vm_runtime_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(error_message, sizeof(error_message), fmt, args);
    va_end(args);

    if (try_depth > 0) {
        longjmp(try_handlers[try_depth - 1].jmp, 1);
    } else if (repl_recovery_jmp != NULL) {
        fprintf(stderr, "Error: %s\n", error_message);
        frame_count = repl_saved_frame_count;
        stack_top = repl_saved_stack_top;
        longjmp(*repl_recovery_jmp, 1);
    } else {
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }
}

static void type_error(const char *context) {
    vm_runtime_error("type error: invalid operand type in %s", context);
}

static int64_t require_num(VMValue v, const char *context) {
    if (v.type != VM_NUM) type_error(context);
    return v.number;
}

static int truthy(VMValue v, const char *context) {
    return require_num(v, context) != 0;
}

static void vm_execute(int stop_at_frame_count) {
    CallFrame *volatile frame = &frames[frame_count - 1];

    static void *dispatch_table[] = {
        &&do_CONST, &&do_ADD, &&do_SUB, &&do_MUL, &&do_DIV, &&do_MOD, &&do_NEG,
        &&do_NOT, &&do_EQ, &&do_NEQ, &&do_LT, &&do_LE, &&do_GT, &&do_GE,
        &&do_AND, &&do_OR,
        &&do_PRINT, &&do_POP,
        &&do_GET_LOCAL, &&do_SET_LOCAL,
        &&do_JUMP, &&do_JUMP_IF_FALSE, &&do_LOOP,
        &&do_CALL, &&do_RETURN,
        &&do_ACC_LOCAL, &&do_CMP_JUMP,
        &&do_ARRAY, &&do_INDEX_GET, &&do_INDEX_SET,
        &&do_MAP, &&do_CALL_VALUE, &&do_CALL_BUILTIN,
        &&do_MAKE_CLOSURE,
        &&do_TRY_PUSH, &&do_TRY_POP
    };

    #define DISPATCH() \
        do { \
            if (frame_count == 1 && frame->ip >= frame->chunk->code + frame->chunk->count) return; \
            goto *dispatch_table[*frame->ip++]; \
        } while (0)

    DISPATCH();

    do_CONST: { uint8_t idx = *frame->ip++; *stack_top++ = frame->chunk->constants[idx]; DISPATCH(); }
    do_ADD: {
        VMValue b = *--stack_top;
        VMValue *a = stack_top - 1;
        if (a->type == VM_STR && b.type == VM_STR) *a = vm_str(vm_concat_strings(a->str, b.str));
        else *a = vm_num(require_num(*a, "'+'") + require_num(b, "'+'"));
        DISPATCH();
    }
    do_SUB: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'-'") - require_num(b, "'-'")); DISPATCH(); }
    do_MUL: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'*'") * require_num(b, "'*'")); DISPATCH(); }
    do_DIV: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'/'") / require_num(b, "'/'")); DISPATCH(); }
    do_MOD: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "'%'") % require_num(b, "'%'")); DISPATCH(); }
    do_NEG: { stack_top[-1] = vm_num(-require_num(stack_top[-1], "unary '-'")); DISPATCH(); }
    do_NOT: { stack_top[-1] = vm_num(require_num(stack_top[-1], "'!'") == 0 ? 1 : 0); DISPATCH(); }
    do_EQ: {
        VMValue b = *--stack_top; VMValue *a = stack_top - 1;
        int eq;
        if (a->type == VM_STR && b.type == VM_STR) eq = strcmp(a->str, b.str) == 0;
        else if (a->type == VM_NUM && b.type == VM_NUM) eq = a->number == b.number;
        else eq = 0;
        *a = vm_num(eq);
        DISPATCH();
    }
    do_NEQ: {
        VMValue b = *--stack_top; VMValue *a = stack_top - 1;
        int eq;
        if (a->type == VM_STR && b.type == VM_STR) eq = strcmp(a->str, b.str) == 0;
        else if (a->type == VM_NUM && b.type == VM_NUM) eq = a->number == b.number;
        else eq = 0;
        *a = vm_num(!eq);
        DISPATCH();
    }
    do_LT: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") < require_num(b, "comparison")); DISPATCH(); }
    do_LE: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") <= require_num(b, "comparison")); DISPATCH(); }
    do_GT: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") > require_num(b, "comparison")); DISPATCH(); }
    do_GE: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num(require_num(*a, "comparison") >= require_num(b, "comparison")); DISPATCH(); }
    do_AND: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((truthy(*a, "'&&'") && truthy(b, "'&&'")) ? 1 : 0); DISPATCH(); }
    do_OR: { VMValue b = *--stack_top; VMValue *a = stack_top - 1; *a = vm_num((truthy(*a, "'||'") || truthy(b, "'||'")) ? 1 : 0); DISPATCH(); }
    do_PRINT: { VMValue v = *--stack_top; vm_print_value(v); printf("\n"); DISPATCH(); }
    do_POP: { --stack_top; DISPATCH(); }
    do_GET_LOCAL: { uint8_t slot = *frame->ip++; *stack_top++ = frame->slots[slot]; DISPATCH(); }
    do_SET_LOCAL: { uint8_t slot = *frame->ip++; frame->slots[slot] = stack_top[-1]; DISPATCH(); }
    do_JUMP: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2; frame->ip += off; DISPATCH();
    }
    do_JUMP_IF_FALSE: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        VMValue cond = *--stack_top;
        if (!truthy(cond, "condition")) frame->ip += off;
        DISPATCH();
    }
    do_LOOP: {
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2; frame->ip -= off; DISPATCH();
    }
    do_CALL: {
        uint8_t func_idx = *frame->ip++;
        uint8_t argc = *frame->ip++;
        VMFunction *fn = &vm_functions[func_idx];
        CallFrame *volatile newf = &frames[frame_count++];
        newf->chunk = &fn->chunk;
        newf->ip = fn->chunk.code;
        newf->slots = stack_top - argc;
        frame = newf;
        DISPATCH();
    }
    do_CALL_VALUE: {
        uint8_t argc = *frame->ip++;
        VMValue callable = *--stack_top;
        VMFunction *fn;
        int extra = 0;
        if (callable.type == VM_FUNCTION) {
            fn = &vm_functions[callable.number];
        } else if (callable.type == VM_CLOSURE) {
            fn = &vm_functions[callable.closure->function_index];
            extra = callable.closure->capture_count;
            for (int i = 0; i < extra; i++) *stack_top++ = callable.closure->captured[i];
        } else {
            type_error("call");
            fn = NULL;
        }
        CallFrame *volatile newf = &frames[frame_count++];
        newf->chunk = &fn->chunk;
        newf->ip = fn->chunk.code;
        newf->slots = stack_top - argc - extra;
        frame = newf;
        DISPATCH();
    }
    do_CALL_BUILTIN: {
        uint8_t builtin_id = *frame->ip++;
        uint8_t argc = *frame->ip++;
        VMValue args[16];
        for (int i = argc - 1; i >= 0; i--) args[i] = *--stack_top;
        *stack_top++ = vm_call_builtin(builtin_id, args, argc);
        DISPATCH();
    }
    do_RETURN: {
        VMValue result = *--stack_top;
        stack_top = frames[frame_count - 1].slots;
        frame_count--;
        *stack_top++ = result;
        frame = &frames[frame_count - 1];
        if (stop_at_frame_count != 0 && frame_count == stop_at_frame_count) return;
        DISPATCH();
    }
    do_ACC_LOCAL: {
        uint8_t slot_a = *frame->ip++;
        uint8_t flags = *frame->ip++;
        uint8_t b_operand = *frame->ip++;
        VMValue b = (flags & 0x80) ? frame->chunk->constants[b_operand] : frame->slots[b_operand];
        int op = flags & 0x03;
        VMValue *a = &frame->slots[slot_a];
        if (op == 0) {
            if (a->type == VM_STR && b.type == VM_STR) *a = vm_str(vm_concat_strings(a->str, b.str));
            else *a = vm_num(require_num(*a, "'+'") + require_num(b, "'+'"));
        } else if (op == 1) {
            *a = vm_num(require_num(*a, "'-'") - require_num(b, "'-'"));
        } else {
            *a = vm_num(require_num(*a, "'*'") * require_num(b, "'*'"));
        }
        DISPATCH();
    }
    do_CMP_JUMP: {
        uint8_t slot_a = *frame->ip++;
        uint8_t flags = *frame->ip++;
        uint8_t b_operand = *frame->ip++;
        uint16_t off = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        VMValue a = frame->slots[slot_a];
        VMValue b = (flags & 0x80) ? frame->chunk->constants[b_operand] : frame->slots[b_operand];
        int cmp_code = flags & 0x0F;
        int taken;
        if (cmp_code == 4 || cmp_code == 5) {
            int eq;
            if (a.type == VM_STR && b.type == VM_STR) eq = strcmp(a.str, b.str) == 0;
            else if (a.type == VM_NUM && b.type == VM_NUM) eq = a.number == b.number;
            else eq = 0;
            taken = (cmp_code == 4) ? eq : !eq;
        } else {
            int64_t av = require_num(a, "comparison");
            int64_t bv = require_num(b, "comparison");
            switch (cmp_code) {
                case 0: taken = av < bv; break;
                case 1: taken = av <= bv; break;
                case 2: taken = av > bv; break;
                default: taken = av >= bv; break;
            }
        }
        if (!taken) frame->ip += off;
        DISPATCH();
    }
    do_ARRAY: {
        uint8_t n = *frame->ip++;
        VMArray *arr = vm_array_new();
        for (int i = 0; i < n; i++) vm_array_push(arr, stack_top[-n + i]);
        stack_top -= n;
        *stack_top++ = vm_array_val(arr);
        DISPATCH();
    }
    do_INDEX_GET: {
        VMValue idx = *--stack_top;
        VMValue arr = *--stack_top;
        if (arr.type == VM_ARRAY) {
            int64_t i = require_num(idx, "array index");
            if (i < 0 || i >= arr.array->count) vm_runtime_error("index out of bounds: %lld", (long long)i);
            *stack_top++ = arr.array->items[i];
        } else if (arr.type == VM_MAP) {
            if (idx.type != VM_STR) vm_runtime_error("map keys must be strings");
            VMValue out;
            if (!vm_map_get(arr.map, idx.str, &out)) vm_runtime_error("key not found: %s", idx.str);
            *stack_top++ = out;
        } else {
            type_error("indexing");
        }
        DISPATCH();
    }
    do_INDEX_SET: {
        VMValue value = *--stack_top;
        VMValue idx = *--stack_top;
        VMValue arr = *--stack_top;
        if (arr.type == VM_ARRAY) {
            int64_t i = require_num(idx, "array index");
            if (i < 0 || i >= arr.array->count) vm_runtime_error("index out of bounds: %lld", (long long)i);
            arr.array->items[i] = value;
        } else if (arr.type == VM_MAP) {
            if (idx.type != VM_STR) vm_runtime_error("map keys must be strings");
            char *key_copy = malloc(strlen(idx.str) + 1); strcpy(key_copy, idx.str);
            vm_map_set(arr.map, key_copy, value);
        } else {
            type_error("index-assign");
        }
        DISPATCH();
    }
    do_MAP: {
        uint8_t n = *frame->ip++;
        VMMap *m = vm_map_new();
        for (int i = 0; i < n; i++) {
            VMValue key = stack_top[-(2 * n) + 2 * i];
            VMValue val = stack_top[-(2 * n) + 2 * i + 1];
            char *key_copy = malloc(strlen(key.str) + 1); strcpy(key_copy, key.str);
            vm_map_set(m, key_copy, val);
        }
        stack_top -= 2 * n;
        *stack_top++ = vm_map_val(m);
        DISPATCH();
    }
    do_MAKE_CLOSURE: {
        uint8_t fn_idx = *frame->ip++;
        uint8_t cap_count = *frame->ip++;
        VMClosureObj *c = malloc(sizeof(VMClosureObj));
        c->function_index = fn_idx;
        c->capture_count = cap_count;
        c->captured = malloc(sizeof(VMValue) * (cap_count > 0 ? cap_count : 1));
        for (int i = 0; i < cap_count; i++) c->captured[i] = frame->slots[i];
        *stack_top++ = vm_closure_val(c);
        DISPATCH();
    }
    do_TRY_PUSH: {
        uint16_t catch_offset = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
        frame->ip += 2;
        if (try_depth >= MAX_TRY_DEPTH) { fprintf(stderr, "too many nested try blocks\n"); exit(1); }
        TryHandler *h = &try_handlers[try_depth];
        h->saved_frame_count = frame_count;
        h->saved_stack_top = stack_top;
        h->target_frame = frame;
        h->target_ip = frame->ip + catch_offset;
        try_depth++;
        if (setjmp(h->jmp) == 0) {
            DISPATCH();
        } else {
            try_depth--;
            frame_count = h->saved_frame_count;
            stack_top = h->saved_stack_top;
            frame = h->target_frame;
            frame->ip = h->target_ip;
            char *msg_copy = malloc(strlen(error_message) + 1);
            strcpy(msg_copy, error_message);
            *stack_top++ = vm_str(msg_copy);
            DISPATCH();
        }
    }
    do_TRY_POP: {
        try_depth--;
        DISPATCH();
    }
}

void vm_run(Chunk *main_chunk) {
    stack_top = stack;
    frame_count = 1;
    frames[0].chunk = main_chunk;
    frames[0].ip = main_chunk->code;
    frames[0].slots = stack;
    vm_execute(0);
}

// Runs bytecode starting at start_offset within the SAME persistent chunk,
// used by the REPL: on the first call, resets the stack/frames like vm_run;
// on later calls, keeps the existing stack/frames (so variables persist) and
// just resumes execution at the newly-appended bytecode's start.
void vm_run_from(Chunk *chunk, int start_offset, int is_first_call) {
    if (is_first_call) {
        stack_top = stack;
        frame_count = 1;
        frames[0].chunk = chunk;
        frames[0].slots = stack;
    }
    frames[0].ip = chunk->code + start_offset;
    vm_execute(0);
}

VMValue vm_call_function_sync(int func_idx, VMValue *args, int argc) {
    for (int i = 0; i < argc; i++) *stack_top++ = args[i];
    int caller_frame_count = frame_count;
    VMFunction *fn = &vm_functions[func_idx];
    CallFrame *volatile newf = &frames[frame_count++];
    newf->chunk = &fn->chunk;
    newf->ip = fn->chunk.code;
    newf->slots = stack_top - argc;
    vm_execute(caller_frame_count);
    return *--stack_top;
}

VMValue vm_call_closure_sync(VMClosureObj *c, VMValue *args, int argc) {
    for (int i = 0; i < argc; i++) *stack_top++ = args[i];
    for (int i = 0; i < c->capture_count; i++) *stack_top++ = c->captured[i];
    int caller_frame_count = frame_count;
    VMFunction *fn = &vm_functions[c->function_index];
    CallFrame *volatile newf = &frames[frame_count++];
    newf->chunk = &fn->chunk;
    newf->ip = fn->chunk.code;
    newf->slots = stack_top - argc - c->capture_count;
    vm_execute(caller_frame_count);
    return *--stack_top;
}

VMValue vm_call_callable_sync(VMValue callable, VMValue *args, int argc) {
    if (callable.type == VM_FUNCTION) return vm_call_function_sync((int)callable.number, args, argc);
    if (callable.type == VM_CLOSURE) return vm_call_closure_sync(callable.closure, args, argc);
    fprintf(stderr, "not callable\n");
    exit(1);
}
