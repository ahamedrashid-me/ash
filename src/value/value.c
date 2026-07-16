#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value/value.h"
#include "value/hashmap.h"
#include "interpreter/error.h"

Value num_val(double n) {
    Value v; v.type = VAL_NUMBER; v.number = n; v.str = NULL; v.array = NULL; v.function_index = -1; v.closure = NULL; v.map = NULL; return v;
}
Value str_val(char *s) {
    Value v; v.type = VAL_STRING; v.number = 0; v.str = s; v.array = NULL; v.function_index = -1; v.closure = NULL; v.map = NULL; return v;
}
Value array_val(ValueArray *a) {
    Value v; v.type = VAL_ARRAY; v.number = 0; v.str = NULL; v.array = a; v.function_index = -1; v.closure = NULL; v.map = NULL; return v;
}
Value func_val(int function_index) {
    Value v; v.type = VAL_FUNCTION; v.number = 0; v.str = NULL; v.array = NULL; v.function_index = function_index; v.closure = NULL; v.map = NULL; return v;
}
Value closure_val(Closure *c) {
    Value v; v.type = VAL_CLOSURE; v.number = 0; v.str = NULL; v.array = NULL; v.function_index = -1; v.closure = c; v.map = NULL; return v;
}
Value map_val(HashMap *m) {
    Value v; v.type = VAL_MAP; v.number = 0; v.str = NULL; v.array = NULL; v.function_index = -1; v.closure = NULL; v.map = m; return v;
}

char *copy_string(const char *start, int length) {
    char *buf = malloc(length + 1);
    memcpy(buf, start, length);
    buf[length] = '\0';
    return buf;
}

char *concat_strings(const char *a, const char *b) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);
    char *buf = malloc(la + lb + 1);
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    buf[la + lb] = '\0';
    return buf;
}

ValueArray *array_new(void) {
    ValueArray *arr = malloc(sizeof(ValueArray));
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void array_push(ValueArray *arr, Value v) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
        arr->items = realloc(arr->items, sizeof(Value) * arr->capacity);
    }
    arr->items[arr->count++] = v;
}

double require_number(Value v, const char *context) {
    if (v.type != VAL_NUMBER) {
        runtime_error("type error: expected number in %s", context);
    }
    return v.number;
}

void print_value(Value v) {
    switch (v.type) {
        case VAL_NUMBER:
            if (v.number == (long long)v.number) printf("%lld", (long long)v.number);
            else printf("%g", v.number);
            break;
        case VAL_STRING:
            printf("%s", v.str);
            break;
        case VAL_ARRAY:
            printf("[");
            for (int i = 0; i < v.array->count; i++) {
                if (v.array->items[i].type == VAL_STRING) printf("\"%s\"", v.array->items[i].str);
                else print_value(v.array->items[i]);
                if (i < v.array->count - 1) printf(", ");
            }
            printf("]");
            break;
        case VAL_FUNCTION:
            printf("<function>");
            break;
        case VAL_CLOSURE:
            printf("<closure>");
            break;
        case VAL_MAP: {
            printf("{");
            int first = 1;
            for (int i = 0; i < v.map->capacity; i++) {
                if (v.map->entries[i].used) {
                    if (!first) printf(", ");
                    printf("\"%s\": ", v.map->entries[i].key);
                    if (v.map->entries[i].value.type == VAL_STRING) printf("\"%s\"", v.map->entries[i].value.str);
                    else print_value(v.map->entries[i].value);
                    first = 0;
                }
            }
            printf("}");
            break;
        }
    }
}

char *copy_string_escaped(const char *start, int length) {
    // worst case output is same size as input (no escape sequences)
    char *buf = malloc(length + 1);
    int out = 0;
    for (int i = 0; i < length; i++) {
        if (start[i] == '\\' && i + 1 < length) {
            char next = start[i + 1];
            char c;
            switch (next) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '0': c = '\0'; break;
                default: c = next; break; // unknown escape: keep the char literally
            }
            buf[out++] = c;
            i++; // skip the escaped character
        } else {
            buf[out++] = start[i];
        }
    }
    buf[out] = '\0';
    return buf;
}
