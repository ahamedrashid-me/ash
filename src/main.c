#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter/statements.h"
#include "interpreter/error.h"
#include "env/env.h"
#include "interpreter/functions.h"

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "could not open file: %s\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static void count_braces(const char *line, int *depth, int *in_string) {
    for (const char *p = line; *p; p++) {
        if (*in_string) {
            if (*p == '\\' && p[1] != '\0') { p++; continue; }
            if (*p == '"') *in_string = 0;
            continue;
        }
        if (*p == '"') { *in_string = 1; continue; }
        if (*p == '{') (*depth)++;
        if (*p == '}') (*depth)--;
    }
}

static int is_blank(const char *s) {
    for (; *s; s++) if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
    return 1;
}

static void run_repl(void) {
    env_reset();
    functions_reset();
    error_init();

    printf("Ash REPL (ashc) -- variables and functions persist between lines.\n");
    printf("Type 'exit' to quit. Runtime errors are caught automatically; syntax errors will end the session.\n\n");

    char buffer[16384];

    for (;;) {
        buffer[0] = '\0';
        int depth = 0;
        int in_string = 0;
        int has_content = 0;

        for (;;) {
            printf(depth == 0 ? "ash> " : "...  ");
            fflush(stdout);

            char line[2048];
            if (!fgets(line, sizeof(line), stdin)) { printf("\n"); return; }

            if (depth == 0 && !has_content) {
                char trimmed[2048];
                strncpy(trimmed, line, sizeof(trimmed) - 1);
                trimmed[sizeof(trimmed) - 1] = '\0';
                size_t l = strlen(trimmed);
                while (l > 0 && (trimmed[l-1] == '\n' || trimmed[l-1] == '\r')) trimmed[--l] = '\0';
                if (strcmp(trimmed, "exit") == 0) return;
            }

            if (!is_blank(line)) has_content = 1;
            count_braces(line, &depth, &in_string);

            strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);

            if (depth <= 0) { depth = 0; break; }
        }

        if (!has_content) continue;

        size_t wrapped_len = strlen(buffer) + 128;
        char *wrapped = malloc(wrapped_len);
        snprintf(wrapped, wrapped_len,
            "try { %s } catch (__repl_error__) { print \"Error: \" + __repl_error__; }",
            buffer);
        interpret_repl_line(wrapped);
        free(wrapped);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        run_repl();
        return 0;
    }
    char *source = read_file(argv[1]);
    error_set_source(source, argv[1]);
    interpret(source);
    free(source);
    return 0;
}
