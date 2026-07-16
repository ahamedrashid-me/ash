#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "builtins/builtins.h"
#include "interpreter/functions.h"
#include "interpreter/closures.h"
#include "value/hashmap.h"
#include "interpreter/error.h"

static const char *builtin_names[] = {
    "len", "push", "input", "str", "num", "sqrt", "abs", "floor",
    "map", "filter", "reduce", "keys", "values", "has", "delete",
    "split", "join", "substring", "indexOf", "replace", "upper", "lower", "trim",
    "read_file", "write_file", "append_file", "file_exists"
};
#define NUM_BUILTINS (int)(sizeof(builtin_names) / sizeof(builtin_names[0]))

int is_builtin(const char *name, int len) {
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if ((int)strlen(builtin_names[i]) == len && strncmp(builtin_names[i], name, len) == 0) return 1;
    }
    return 0;
}

static Value call_any(Value fn, Value *args, int argc) {
    if (fn.type == VAL_FUNCTION) return invoke_function_by_index(fn.function_index, args, argc);
    if (fn.type == VAL_CLOSURE) return invoke_closure(fn.closure, args, argc);
    runtime_error("expected a function or closure");
}

Value call_builtin(const char *name, int len, Value *args, int argc) {
    if (len == 3 && strncmp(name, "len", 3) == 0) {
        if (argc != 1) { runtime_error("len() expects 1 argument"); exit(1); }
        if (args[0].type == VAL_STRING) return num_val((double)strlen(args[0].str));
        if (args[0].type == VAL_ARRAY) return num_val((double)args[0].array->count);
        if (args[0].type == VAL_MAP) return num_val((double)args[0].map->count);
        runtime_error("len() expects a string, array, or map");
    }
    if (len == 4 && strncmp(name, "push", 4) == 0) {
        if (argc != 2 || args[0].type != VAL_ARRAY) { runtime_error("push(array, value) expected"); }
        array_push(args[0].array, args[1]);
        return args[0];
    }
    if (len == 5 && strncmp(name, "input", 5) == 0) {
        char buf[1024];
        if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
        size_t l = strlen(buf);
        if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
        return str_val(copy_string(buf, (int)strlen(buf)));
    }
    if (len == 3 && strncmp(name, "str", 3) == 0) {
        if (argc != 1 || args[0].type != VAL_NUMBER) { runtime_error("str() expects a number"); }
        char buf[64];
        if (args[0].number == (long long)args[0].number) snprintf(buf, sizeof(buf), "%lld", (long long)args[0].number);
        else snprintf(buf, sizeof(buf), "%g", args[0].number);
        return str_val(copy_string(buf, (int)strlen(buf)));
    }
    if (len == 3 && strncmp(name, "num", 3) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("num() expects a string"); }
        return num_val(strtod(args[0].str, NULL));
    }
    if (len == 4 && strncmp(name, "sqrt", 4) == 0) {
        if (argc != 1 || args[0].type != VAL_NUMBER) { runtime_error("sqrt() expects a number"); }
        return num_val(sqrt(args[0].number));
    }
    if (len == 3 && strncmp(name, "abs", 3) == 0) {
        if (argc != 1 || args[0].type != VAL_NUMBER) { runtime_error("abs() expects a number"); }
        return num_val(fabs(args[0].number));
    }
    if (len == 5 && strncmp(name, "floor", 5) == 0) {
        if (argc != 1 || args[0].type != VAL_NUMBER) { runtime_error("floor() expects a number"); }
        return num_val(floor(args[0].number));
    }
    if (len == 3 && strncmp(name, "map", 3) == 0) {
        if (argc != 2 || args[0].type != VAL_ARRAY || (args[1].type != VAL_FUNCTION && args[1].type != VAL_CLOSURE)) {
            runtime_error("map(array, fn) expected");
        }
        ValueArray *result = array_new();
        for (int i = 0; i < args[0].array->count; i++) {
            Value call_args[1] = { args[0].array->items[i] };
            array_push(result, call_any(args[1], call_args, 1));
        }
        return array_val(result);
    }
    if (len == 6 && strncmp(name, "filter", 6) == 0) {
        if (argc != 2 || args[0].type != VAL_ARRAY || (args[1].type != VAL_FUNCTION && args[1].type != VAL_CLOSURE)) {
            runtime_error("filter(array, fn) expected");
        }
        ValueArray *result = array_new();
        for (int i = 0; i < args[0].array->count; i++) {
            Value call_args[1] = { args[0].array->items[i] };
            Value keep = call_any(args[1], call_args, 1);
            if (require_number(keep, "filter predicate") != 0.0) array_push(result, args[0].array->items[i]);
        }
        return array_val(result);
    }
    if (len == 6 && strncmp(name, "reduce", 6) == 0) {
        if (argc != 3 || args[0].type != VAL_ARRAY || (args[1].type != VAL_FUNCTION && args[1].type != VAL_CLOSURE)) {
            runtime_error("reduce(array, fn, initial) expected");
        }
        Value acc = args[2];
        for (int i = 0; i < args[0].array->count; i++) {
            Value call_args[2] = { acc, args[0].array->items[i] };
            acc = call_any(args[1], call_args, 2);
        }
        return acc;
    }
    if (len == 4 && strncmp(name, "keys", 4) == 0) {
        if (argc != 1 || args[0].type != VAL_MAP) { runtime_error("keys() expects a map"); }
        ValueArray *result = array_new();
        for (int i = 0; i < args[0].map->capacity; i++) {
            if (args[0].map->entries[i].used) {
                array_push(result, str_val(copy_string(args[0].map->entries[i].key, (int)strlen(args[0].map->entries[i].key))));
            }
        }
        return array_val(result);
    }
    if (len == 6 && strncmp(name, "values", 6) == 0) {
        if (argc != 1 || args[0].type != VAL_MAP) { runtime_error("values() expects a map"); }
        ValueArray *result = array_new();
        for (int i = 0; i < args[0].map->capacity; i++) {
            if (args[0].map->entries[i].used) array_push(result, args[0].map->entries[i].value);
        }
        return array_val(result);
    }
    if (len == 3 && strncmp(name, "has", 3) == 0) {
        if (argc != 2 || args[0].type != VAL_MAP || args[1].type != VAL_STRING) { runtime_error("has(map, key) expected"); }
        return num_val(map_has(args[0].map, args[1].str) ? 1 : 0);
    }
    if (len == 6 && strncmp(name, "delete", 6) == 0) {
        if (argc != 2 || args[0].type != VAL_MAP || args[1].type != VAL_STRING) { runtime_error("delete(map, key) expected"); }
        map_delete(args[0].map, args[1].str);
        return args[0];
    }
    if (len == 5 && strncmp(name, "split", 5) == 0) {
        if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) { runtime_error("split(str, delim) expected"); }
        const char *s = args[0].str;
        const char *delim = args[1].str;
        int dlen = (int)strlen(delim);
        ValueArray *result = array_new();
        if (dlen == 0) {
            int slen = (int)strlen(s);
            for (int i = 0; i < slen; i++) array_push(result, str_val(copy_string(s + i, 1)));
            return array_val(result);
        }
        const char *cur = s;
        const char *found;
        while ((found = strstr(cur, delim)) != NULL) {
            array_push(result, str_val(copy_string(cur, (int)(found - cur))));
            cur = found + dlen;
        }
        array_push(result, str_val(copy_string(cur, (int)strlen(cur))));
        return array_val(result);
    }
    if (len == 4 && strncmp(name, "join", 4) == 0) {
        if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STRING) { runtime_error("join(array, delim) expected"); }
        int total = 0;
        int dlen = (int)strlen(args[1].str);
        for (int i = 0; i < args[0].array->count; i++) {
            if (args[0].array->items[i].type != VAL_STRING) { runtime_error("join() expects an array of strings"); }
            total += (int)strlen(args[0].array->items[i].str);
            if (i < args[0].array->count - 1) total += dlen;
        }
        char *buf = malloc(total + 1);
        buf[0] = '\0';
        for (int i = 0; i < args[0].array->count; i++) {
            strcat(buf, args[0].array->items[i].str);
            if (i < args[0].array->count - 1) strcat(buf, args[1].str);
        }
        return str_val(buf);
    }
    if (len == 9 && strncmp(name, "substring", 9) == 0) {
        if (argc != 3 || args[0].type != VAL_STRING) { runtime_error("substring(str, start, end) expected"); }
        int slen = (int)strlen(args[0].str);
        int start = (int)require_number(args[1], "substring start");
        int end = (int)require_number(args[2], "substring end");
        if (start < 0) start = 0;
        if (end > slen) end = slen;
        if (start > end) start = end;
        return str_val(copy_string(args[0].str + start, end - start));
    }
    if (len == 7 && strncmp(name, "indexOf", 7) == 0) {
        if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) { runtime_error("indexOf(str, search) expected"); }
        char *found = strstr(args[0].str, args[1].str);
        if (!found) return num_val(-1);
        return num_val((double)(found - args[0].str));
    }
    if (len == 7 && strncmp(name, "replace", 7) == 0) {
        if (argc != 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
            runtime_error("replace(str, search, replacement) expected");
        }
        const char *search = args[1].str;
        int search_len = (int)strlen(search);
        if (search_len == 0) return str_val(copy_string(args[0].str, (int)strlen(args[0].str)));
        int cap = (int)strlen(args[0].str) * 2 + 16;
        char *buf = malloc(cap);
        int blen = 0;
        const char *cur = args[0].str;
        const char *found;
        int rep_len = (int)strlen(args[2].str);
        while ((found = strstr(cur, search)) != NULL) {
            int prefix = (int)(found - cur);
            while (blen + prefix + rep_len + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
            memcpy(buf + blen, cur, prefix); blen += prefix;
            memcpy(buf + blen, args[2].str, rep_len); blen += rep_len;
            cur = found + search_len;
        }
        int remaining = (int)strlen(cur);
        while (blen + remaining + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + blen, cur, remaining); blen += remaining;
        buf[blen] = '\0';
        return str_val(buf);
    }
    if (len == 5 && strncmp(name, "upper", 5) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("upper() expects a string"); }
        char *copy = copy_string(args[0].str, (int)strlen(args[0].str));
        for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
        return str_val(copy);
    }
    if (len == 5 && strncmp(name, "lower", 5) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("lower() expects a string"); }
        char *copy = copy_string(args[0].str, (int)strlen(args[0].str));
        for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
        return str_val(copy);
    }
    if (len == 4 && strncmp(name, "trim", 4) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("trim() expects a string"); }
        const char *s = args[0].str;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        int slen = (int)strlen(s);
        while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t' || s[slen - 1] == '\n' || s[slen - 1] == '\r')) slen--;
        return str_val(copy_string(s, slen));
    }
    if (len == 9 && strncmp(name, "read_file", 9) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("read_file(path) expected"); }
        FILE *f = fopen(args[0].str, "rb");
        if (!f) { runtime_error("could not open file: %s", args[0].str); }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        char *buf = malloc(size + 1);
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
        fclose(f);
        return str_val(buf);
    }
    if (len == 10 && strncmp(name, "write_file", 10) == 0) {
        if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) { runtime_error("write_file(path, content) expected"); }
        FILE *f = fopen(args[0].str, "wb");
        if (!f) { runtime_error("could not write file: %s", args[0].str); }
        fwrite(args[1].str, 1, strlen(args[1].str), f);
        fclose(f);
        return num_val(1);
    }
    if (len == 11 && strncmp(name, "append_file", 11) == 0) {
        if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) { runtime_error("append_file(path, content) expected"); }
        FILE *f = fopen(args[0].str, "ab");
        if (!f) { runtime_error("could not open file for append: %s", args[0].str); }
        fwrite(args[1].str, 1, strlen(args[1].str), f);
        fclose(f);
        return num_val(1);
    }
    if (len == 11 && strncmp(name, "file_exists", 11) == 0) {
        if (argc != 1 || args[0].type != VAL_STRING) { runtime_error("file_exists(path) expected"); }
        FILE *f = fopen(args[0].str, "rb");
        if (f) { fclose(f); return num_val(1); }
        return num_val(0);
    }
    runtime_error("unknown builtin: %.*s", len, name);
}
