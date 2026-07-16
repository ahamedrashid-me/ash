#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "vm/builtins.h"
#include "vm/hashmap.h"
#include "vm/vm.h"

static const char *builtin_names[] = {
    "len", "push", "input", "str", "num", "sqrt", "abs", "floor",
    "map", "filter", "reduce", "keys", "values", "has", "delete",
    "split", "join", "substring", "indexOf", "replace", "upper", "lower", "trim",
    "read_file", "write_file", "append_file", "file_exists"
};
#define NUM_BUILTINS (int)(sizeof(builtin_names) / sizeof(builtin_names[0]))

int vm_builtin_lookup(const char *name, int len) {
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if ((int)strlen(builtin_names[i]) == len && strncmp(builtin_names[i], name, len) == 0) return i;
    }
    return -1;
}

VMValue vm_call_builtin(int id, VMValue *args, int argc) {
    switch (id) {
        case 0: { // len
            if (argc != 1) { vm_runtime_error("len() expects 1 argument\n"); }
            if (args[0].type == VM_STR) return vm_num((int64_t)strlen(args[0].str));
            if (args[0].type == VM_ARRAY) return vm_num(args[0].array->count);
            if (args[0].type == VM_MAP) return vm_num(args[0].map->count);
            vm_runtime_error("len() expects a string, array, or map\n");
        }
        case 1: { // push
            if (argc != 2 || args[0].type != VM_ARRAY) { vm_runtime_error("push(array, value) expected\n"); }
            vm_array_push(args[0].array, args[1]);
            return args[0];
        }
        case 2: { // input
            char buf[1024];
            if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
            size_t l = strlen(buf);
            if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
            char *copy = malloc(strlen(buf) + 1); strcpy(copy, buf);
            return vm_str(copy);
        }
        case 3: { // str
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("str() expects a number\n"); }
            char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)args[0].number);
            char *copy = malloc(strlen(buf) + 1); strcpy(copy, buf);
            return vm_str(copy);
        }
        case 4: { // num
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("num() expects a string\n"); }
            return vm_num((int64_t)strtoll(args[0].str, NULL, 10));
        }
        case 5: { // sqrt (truncates to integer -- the VM is integer-only)
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("sqrt() expects a number\n"); }
            return vm_num((int64_t)sqrt((double)args[0].number));
        }
        case 6: { // abs
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("abs() expects a number\n"); }
            int64_t n = args[0].number;
            return vm_num(n < 0 ? -n : n);
        }
        case 7: { // floor -- no-op, VM numbers are already integers
            if (argc != 1 || args[0].type != VM_NUM) { vm_runtime_error("floor() expects a number\n"); }
            return args[0];
        }
        case 8: { // map
            if (argc != 2 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("map(array, fn) expected\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[1] = { args[0].array->items[i] };
                vm_array_push(result, vm_call_callable_sync(args[1], call_args, 1));
            }
            return vm_array_val(result);
        }
        case 9: { // filter
            if (argc != 2 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("filter(array, fn) expected\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[1] = { args[0].array->items[i] };
                VMValue keep = vm_call_callable_sync(args[1], call_args, 1);
                if (keep.type == VM_NUM && keep.number != 0) vm_array_push(result, args[0].array->items[i]);
            }
            return vm_array_val(result);
        }
        case 10: { // reduce
            if (argc != 3 || args[0].type != VM_ARRAY || (args[1].type != VM_FUNCTION && args[1].type != VM_CLOSURE)) { vm_runtime_error("reduce(array, fn, initial) expected\n"); }
            VMValue acc = args[2];
            for (int i = 0; i < args[0].array->count; i++) {
                VMValue call_args[2] = { acc, args[0].array->items[i] };
                acc = vm_call_callable_sync(args[1], call_args, 2);
            }
            return acc;
        }
        case 11: { // keys
            if (argc != 1 || args[0].type != VM_MAP) { vm_runtime_error("keys() expects a map\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].map->capacity; i++) {
                if (args[0].map->entries[i].used) {
                    char *k = args[0].map->entries[i].key;
                    char *copy = malloc(strlen(k) + 1); strcpy(copy, k);
                    vm_array_push(result, vm_str(copy));
                }
            }
            return vm_array_val(result);
        }
        case 12: { // values
            if (argc != 1 || args[0].type != VM_MAP) { vm_runtime_error("values() expects a map\n"); }
            VMArray *result = vm_array_new();
            for (int i = 0; i < args[0].map->capacity; i++) {
                if (args[0].map->entries[i].used) vm_array_push(result, args[0].map->entries[i].value);
            }
            return vm_array_val(result);
        }
        case 13: { // has
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("has(map, key) expected\n"); }
            return vm_num(vm_map_has(args[0].map, args[1].str) ? 1 : 0);
        }
        case 14: { // delete
            if (argc != 2 || args[0].type != VM_MAP || args[1].type != VM_STR) { vm_runtime_error("delete(map, key) expected\n"); }
            vm_map_delete(args[0].map, args[1].str);
            return args[0];
        }
        case 15: { // split
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("split(str, delim) expected\n"); }
            const char *s = args[0].str; const char *delim = args[1].str;
            int dlen = (int)strlen(delim);
            VMArray *result = vm_array_new();
            if (dlen == 0) {
                int slen = (int)strlen(s);
                for (int i = 0; i < slen; i++) { char *c = malloc(2); c[0] = s[i]; c[1] = '\0'; vm_array_push(result, vm_str(c)); }
                return vm_array_val(result);
            }
            const char *cur = s; const char *found;
            while ((found = strstr(cur, delim)) != NULL) {
                int len = (int)(found - cur);
                char *piece = malloc(len + 1); memcpy(piece, cur, len); piece[len] = '\0';
                vm_array_push(result, vm_str(piece));
                cur = found + dlen;
            }
            char *piece = malloc(strlen(cur) + 1); strcpy(piece, cur);
            vm_array_push(result, vm_str(piece));
            return vm_array_val(result);
        }
        case 16: { // join
            if (argc != 2 || args[0].type != VM_ARRAY || args[1].type != VM_STR) { vm_runtime_error("join(array, delim) expected\n"); }
            int total = 0; int dlen = (int)strlen(args[1].str);
            for (int i = 0; i < args[0].array->count; i++) {
                if (args[0].array->items[i].type != VM_STR) { vm_runtime_error("join() expects an array of strings\n"); }
                total += (int)strlen(args[0].array->items[i].str);
                if (i < args[0].array->count - 1) total += dlen;
            }
            char *buf = malloc(total + 1); buf[0] = '\0';
            for (int i = 0; i < args[0].array->count; i++) {
                strcat(buf, args[0].array->items[i].str);
                if (i < args[0].array->count - 1) strcat(buf, args[1].str);
            }
            return vm_str(buf);
        }
        case 17: { // substring
            if (argc != 3 || args[0].type != VM_STR) { vm_runtime_error("substring(str, start, end) expected\n"); }
            int slen = (int)strlen(args[0].str);
            int start = (int)args[1].number; int end = (int)args[2].number;
            if (start < 0) start = 0;
            if (end > slen) end = slen;
            if (start > end) start = end;
            char *buf = malloc(end - start + 1); memcpy(buf, args[0].str + start, end - start); buf[end - start] = '\0';
            return vm_str(buf);
        }
        case 18: { // indexOf
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("indexOf(str, search) expected\n"); }
            char *found = strstr(args[0].str, args[1].str);
            if (!found) return vm_num(-1);
            return vm_num(found - args[0].str);
        }
        case 19: { // replace
            if (argc != 3 || args[0].type != VM_STR || args[1].type != VM_STR || args[2].type != VM_STR) { vm_runtime_error("replace(str, search, replacement) expected\n"); }
            const char *search = args[1].str; int search_len = (int)strlen(search);
            if (search_len == 0) { char *c = malloc(strlen(args[0].str) + 1); strcpy(c, args[0].str); return vm_str(c); }
            int cap = (int)strlen(args[0].str) * 2 + 16; char *buf = malloc(cap); int blen = 0;
            const char *cur = args[0].str; const char *found; int rep_len = (int)strlen(args[2].str);
            while ((found = strstr(cur, search)) != NULL) {
                int prefix = (int)(found - cur);
                while (blen + prefix + rep_len + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
                memcpy(buf + blen, cur, prefix); blen += prefix;
                memcpy(buf + blen, args[2].str, rep_len); blen += rep_len;
                cur = found + search_len;
            }
            int remaining = (int)strlen(cur);
            while (blen + remaining + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
            memcpy(buf + blen, cur, remaining); blen += remaining; buf[blen] = '\0';
            return vm_str(buf);
        }
        case 20: { // upper
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("upper() expects a string\n"); }
            char *copy = malloc(strlen(args[0].str) + 1); strcpy(copy, args[0].str);
            for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
            return vm_str(copy);
        }
        case 21: { // lower
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("lower() expects a string\n"); }
            char *copy = malloc(strlen(args[0].str) + 1); strcpy(copy, args[0].str);
            for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
            return vm_str(copy);
        }
        case 22: { // trim
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("trim() expects a string\n"); }
            const char *s = args[0].str;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            int slen = (int)strlen(s);
            while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t' || s[slen - 1] == '\n' || s[slen - 1] == '\r')) slen--;
            char *buf = malloc(slen + 1); memcpy(buf, s, slen); buf[slen] = '\0';
            return vm_str(buf);
        }
        case 23: { // read_file
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("read_file(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (!f) { vm_runtime_error("could not open file: %s\n", args[0].str); }
            fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
            char *buf = malloc(size + 1); size_t n = fread(buf, 1, size, f); buf[n] = '\0'; fclose(f);
            return vm_str(buf);
        }
        case 24: { // write_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("write_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "wb");
            if (!f) { vm_runtime_error("could not write file: %s\n", args[0].str); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_num(1);
        }
        case 25: { // append_file
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("append_file(path, content) expected\n"); }
            FILE *f = fopen(args[0].str, "ab");
            if (!f) { vm_runtime_error("could not open file for append: %s\n", args[0].str); }
            fwrite(args[1].str, 1, strlen(args[1].str), f); fclose(f);
            return vm_num(1);
        }
        case 26: { // file_exists
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("file_exists(path) expected\n"); }
            FILE *f = fopen(args[0].str, "rb");
            if (f) { fclose(f); return vm_num(1); }
            return vm_num(0);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
