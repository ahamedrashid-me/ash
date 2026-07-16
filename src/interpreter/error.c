#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "interpreter/error.h"
#include "interpreter/parser_state.h"

#define MAX_TRY_DEPTH 32

static jmp_buf handlers[MAX_TRY_DEPTH];
static int try_depth = 0;
static char last_message[512];

static const char *g_source = NULL;
static const char *g_filename = "source";

#define COL_RED    "\033[38;5;203m"
#define COL_CYAN   "\033[38;5;117m"
#define COL_GRAY   "\033[38;5;242m"
#define COL_GREEN  "\033[38;5;150m"
#define COL_RESET  "\033[0m"
#define COL_BOLD   "\033[1m"

void error_init(void) {
    try_depth = 0;
    last_message[0] = '\0';
}

void error_set_source(const char *source, const char *filename) {
    g_source = source;
    g_filename = filename;
}

jmp_buf *error_push_handler(void) {
    if (try_depth >= MAX_TRY_DEPTH) {
        fprintf(stderr, "too many nested try blocks\n");
        exit(1);
    }
    return &handlers[try_depth++];
}

void error_pop_handler(void) {
    if (try_depth > 0) try_depth--;
}

const char *error_last_message(void) {
    return last_message;
}

static int use_color(void) {
    return isatty(fileno(stderr));
}

// Computes the TRUE line/column by scanning the original, never-re-lexed source
// buffer from the start — sidesteps the fact that the interpreter's internal line
// counter resets every time it jumps into a function body's re-lexed tokens.
static void compute_line_col(const char *pos, int *line, int *col) {
    int l = 1, c = 1;
    const char *p = g_source;
    while (p < pos && *p) {
        if (*p == '\n') { l++; c = 1; } else { c++; }
        p++;
    }
    *line = l;
    *col = c;
}

static void get_source_line(const char *pos, const char **line_start, int *line_len) {
    const char *start = pos;
    while (start > g_source && start[-1] != '\n') start--;
    const char *end = pos;
    while (*end && *end != '\n') end++;
    *line_start = start;
    *line_len = (int)(end - start);
}

static int has_prefix(const char *message, const char *prefix) {
    return strncmp(message, prefix, strlen(prefix)) == 0;
}

static const char *lookup_hint(const char *message) {
    if (has_prefix(message, "undefined variable")) return "did you forget to declare it with `let`?";
    if (has_prefix(message, "undefined function")) return "check the function name is spelled correctly";
    if (has_prefix(message, "index out of bounds")) return "check the array's length before indexing";
    if (has_prefix(message, "key not found")) return "use `has(map, key)` to check before accessing";
    if (has_prefix(message, "could not open file")) return "check the file path is correct";
    if (has_prefix(message, "type error")) return "check the value's type before using it this way";
    if (strstr(message, "expected") != NULL) return "check for a missing token nearby, like `;`, `)`, or `}`";
    return NULL;
}

static void print_pretty_error(const char *label, const char *message) {
    int color = use_color();
    const char *pos = previous.start;
    int len = previous.length > 0 ? previous.length : 1;

    if (!g_source || !pos) {
        fprintf(stderr, "%s: %s\n", label, message);
        return;
    }

    int line, col;
    compute_line_col(pos, &line, &col);
    const char *line_start; int line_len;
    get_source_line(pos, &line_start, &line_len);

    char linenum_buf[16];
    snprintf(linenum_buf, sizeof(linenum_buf), "%d", line);
    int gutter_width = (int)strlen(linenum_buf);

    if (color) {
        fprintf(stderr, COL_BOLD COL_RED "%s" COL_RESET COL_BOLD ": %s" COL_RESET "\n", label, message);
        fprintf(stderr, COL_CYAN "  --> %s:%d:%d" COL_RESET "\n", g_filename, line, col);
        fprintf(stderr, COL_GRAY "%*s |" COL_RESET "\n", gutter_width, "");
        fprintf(stderr, COL_GRAY "%s |" COL_RESET " %.*s\n", linenum_buf, line_len, line_start);
        fprintf(stderr, COL_GRAY "%*s |" COL_RESET " ", gutter_width, "");
        for (int i = 1; i < col; i++) fputc(' ', stderr);
        fprintf(stderr, COL_RED);
        for (int i = 0; i < len; i++) fputc('^', stderr);
        fprintf(stderr, COL_RESET "\n");
        const char *hint = lookup_hint(message);
        if (hint) {
            fprintf(stderr, COL_GRAY "%*s |" COL_RESET "\n", gutter_width, "");
            fprintf(stderr, "  = " COL_GREEN COL_BOLD "help" COL_RESET ": %s\n", hint);
        }
    } else {
        fprintf(stderr, "%s: %s\n", label, message);
        fprintf(stderr, "  --> %s:%d:%d\n", g_filename, line, col);
        fprintf(stderr, "%*s |\n", gutter_width, "");
        fprintf(stderr, "%s | %.*s\n", linenum_buf, line_len, line_start);
        fprintf(stderr, "%*s | ", gutter_width, "");
        for (int i = 1; i < col; i++) fputc(' ', stderr);
        for (int i = 0; i < len; i++) fputc('^', stderr);
        fputc('\n', stderr);
        const char *hint = lookup_hint(message);
        if (hint) {
            fprintf(stderr, "%*s |\n", gutter_width, "");
            fprintf(stderr, "  = help: %s\n", hint);
        }
    }
}

void runtime_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_message, sizeof(last_message), fmt, args);
    va_end(args);

    if (try_depth > 0) {
        longjmp(handlers[try_depth - 1], 1);
    } else {
        print_pretty_error("error", last_message);
        exit(1);
    }
}

void parse_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    print_pretty_error("error", buf);
    exit(1);
}
