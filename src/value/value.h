#ifndef ASH_VALUE_H
#define ASH_VALUE_H

typedef enum { VAL_NUMBER, VAL_STRING, VAL_ARRAY, VAL_FUNCTION, VAL_CLOSURE, VAL_MAP } ValueType;

typedef struct ValueArray ValueArray;
typedef struct Closure Closure;
typedef struct HashMap HashMap;

typedef struct Value {
    ValueType type;
    double number;
    char *str;
    ValueArray *array;
    int function_index;
    Closure *closure;
    HashMap *map;
} Value;

struct ValueArray {
    Value *items;
    int count;
    int capacity;
};

Value num_val(double n);
Value str_val(char *s);
Value array_val(ValueArray *a);
Value func_val(int function_index);
Value closure_val(Closure *c);
Value map_val(HashMap *m);

char *copy_string(const char *start, int length);
char *copy_string_escaped(const char *start, int length);
char *concat_strings(const char *a, const char *b);

ValueArray *array_new(void);
void array_push(ValueArray *arr, Value v);

double require_number(Value v, const char *context);
void print_value(Value v);

#endif
