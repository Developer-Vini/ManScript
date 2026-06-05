#include "../include/mz.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MZ_GC_THRESHOLD 20000

typedef enum
{
    TK_EOF,
    TK_ID,
    TK_NUM,
    TK_STR,
    TK_LET,
    TK_FUN,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
    TK_SWITCH,
    TK_CASE,
    TK_DEFAULT,
    TK_BREAK,
    TK_CONTINUE,
    TK_TRUE,
    TK_FALSE,
    TK_NULL,
    TK_AND,
    TK_OR,
    TK_NOT,
    TK_PLUS,
    TK_PLUSPLUS,
    TK_MINUS,
    TK_MINUSMINUS,
    TK_STAR,
    TK_SLASH,
    TK_PERCENT,
    TK_EQ,
    TK_PLUSEQ,
    TK_MINUSEQ,
    TK_STAREQ,
    TK_SLASHEQ,
    TK_PERCENTEQ,
    TK_EQEQ,
    TK_BANG,
    TK_BANGEQ,
    TK_LT,
    TK_LTEQ,
    TK_GT,
    TK_GTEQ,
    TK_LP,
    TK_RP,
    TK_LB,
    TK_RB,
    TK_LC,
    TK_RC,
    TK_COMMA,
    TK_DOT,
    TK_COLON,
    TK_SEMI
} TokKind;

typedef struct
{
    TokKind kind;
    const char *s;
    int len;
    int line;
    double number;
} Tok;

typedef enum
{
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_OBJECT,
    OBJ_FUNCTION
} ObjKind;

typedef struct Obj Obj;
typedef struct Env Env;

typedef struct
{
    int count;
    int cap;
    MzValue *items;
} MzArray;

typedef struct
{
    char *key;
    MzValue value;
} Field;

typedef struct
{
    int count;
    int cap;
    Field *fields;
} MzObject;

typedef struct
{
    char *name;
    int arity;
    char **params;
    int body_start;
    int body_end;
    Env *closure;
} MzFunction;

struct Obj
{
    ObjKind kind;
    int marked;
    Obj *next;
    union
    {
        struct
        {
            int len;
            char *chars;
        } string;
        MzArray array;
        MzObject object;
        MzFunction function;
    } as;
};

typedef struct
{
    char *name;
    MzValue value;
} Binding;

struct Env
{
    Env *parent;
    Env *next;
    int marked;
    int count;
    int cap;
    Binding *items;
};

struct MzVM
{
    Tok *tokens;
    int token_count;
    int token_cap;
    int pos;
    int end;
    Env *env;
    Env *globals;
    Env *envs;
    Obj *objects;
    int allocs;
    int call_depth;
    int returning;
    int breaking;
    int continuing;
    MzValue return_value;
    char error[512];
    int failed;
};

typedef struct
{
    MzVM *vm;
    int pos;
    int end;
    Env *env;
} Parser;

typedef struct
{
    int type;
    char *name;
    MzValue base;
    MzValue index;
    MzValue current;
} LValue;

static MzValue expression(Parser *p);
static void statement(Parser *p);
static void var_statement(Parser *p);
static void expr_statement(Parser *p);
static MzValue call_value(MzVM *vm, Env *fallback, MzValue callee, int argc, MzValue *argv, MzValue thisv, int has_this);
static char *read_file(const char *path);

static void *xrealloc(void *p, size_t n)
{
    void *r = realloc(p, n);
    if (!r)
        exit(1);
    return r;
}

static char *copy_span(const char *s, int n)
{
    char *out = (char *)malloc((size_t)n + 1);
    if (!out)
        exit(1);
    memcpy(out, s, (size_t)n);
    out[n] = 0;
    return out;
}

MzValue mz_null(void)
{
    MzValue v;
    v.type = MZ_NULL;
    return v;
}
MzValue mz_number(double n)
{
    MzValue v;
    v.type = MZ_NUMBER;
    v.as.number = n;
    return v;
}
MzValue mz_bool(int b)
{
    MzValue v;
    v.type = MZ_BOOL;
    v.as.boolean = b != 0;
    return v;
}
static MzValue obj_value(Obj *o, MzType t)
{
    MzValue v;
    v.type = t;
    v.as.ptr = o;
    return v;
}
static MzValue native_value(MzNative fn)
{
    MzValue v;
    v.type = MZ_NATIVE;
    v.as.native = fn;
    return v;
}

double mz_as_number(MzValue v)
{
    if (v.type == MZ_NUMBER)
        return v.as.number;
    if (v.type == MZ_BOOL)
        return v.as.boolean ? 1.0 : 0.0;
    if (v.type == MZ_STRING)
        return strtod(((Obj *)v.as.ptr)->as.string.chars, NULL);
    return 0.0;
}

int mz_truthy(MzValue v)
{
    if (v.type == MZ_NULL)
        return 0;
    if (v.type == MZ_BOOL)
        return v.as.boolean;
    if (v.type == MZ_NUMBER)
        return v.as.number != 0.0;
    if (v.type == MZ_STRING)
        return ((Obj *)v.as.ptr)->as.string.len > 0;
    return 1;
}

static Obj *new_obj(MzVM *vm, ObjKind kind)
{
    Obj *o = (Obj *)calloc(1, sizeof(Obj));
    if (!o)
        exit(1);
    o->kind = kind;
    o->next = vm->objects;
    vm->objects = o;
    vm->allocs++;
    return o;
}

MzValue mz_string(MzVM *vm, const char *s)
{
    int n = (int)strlen(s);
    Obj *o = new_obj(vm, OBJ_STRING);
    o->as.string.len = n;
    o->as.string.chars = copy_span(s, n);
    return obj_value(o, MZ_STRING);
}

static MzValue string_take(MzVM *vm, char *s)
{
    Obj *o = new_obj(vm, OBJ_STRING);
    o->as.string.len = (int)strlen(s);
    o->as.string.chars = s;
    return obj_value(o, MZ_STRING);
}

static MzValue array_new(MzVM *vm)
{
    Obj *o = new_obj(vm, OBJ_ARRAY);
    return obj_value(o, MZ_ARRAY);
}

static MzValue object_new(MzVM *vm)
{
    Obj *o = new_obj(vm, OBJ_OBJECT);
    return obj_value(o, MZ_OBJECT);
}

static MzValue function_new(MzVM *vm, const char *name, int arity)
{
    Obj *o = new_obj(vm, OBJ_FUNCTION);
    o->as.function.name = copy_span(name, (int)strlen(name));
    o->as.function.arity = arity;
    if (arity > 0)
    {
        o->as.function.params = (char **)calloc((size_t)arity, sizeof(char *));
        if (!o->as.function.params)
            exit(1);
    }
    return obj_value(o, MZ_FUNCTION);
}

static void array_push(MzArray *a, MzValue v)
{
    if (a->count + 1 > a->cap)
    {
        a->cap = a->cap < 8 ? 8 : a->cap * 2;
        a->items = (MzValue *)xrealloc(a->items, sizeof(MzValue) * (size_t)a->cap);
    }
    a->items[a->count++] = v;
}

static MzValue array_get(MzArray *a, int i)
{
    if (i < 0 || i >= a->count)
        return mz_null();
    return a->items[i];
}

int mz_array_len(MzValue v)
{
    if (v.type != MZ_ARRAY)
        return 0;
    return ((Obj *)v.as.ptr)->as.array.count;
}

MzValue mz_array_get(MzValue v, int index)
{
    if (v.type != MZ_ARRAY)
        return mz_null();
    return array_get(&((Obj *)v.as.ptr)->as.array, index);
}

static void array_set(MzArray *a, int i, MzValue v)
{
    if (i < 0)
        return;
    while (i >= a->count)
        array_push(a, mz_null());
    a->items[i] = v;
}

static int field_find(MzObject *o, const char *key)
{
    for (int i = 0; i < o->count; i++)
        if (!strcmp(o->fields[i].key, key))
            return i;
    return -1;
}

static MzValue object_get(MzObject *o, const char *key)
{
    int i = field_find(o, key);
    return i >= 0 ? o->fields[i].value : mz_null();
}

static void object_set(MzObject *o, const char *key, MzValue v)
{
    int i = field_find(o, key);
    if (i >= 0)
    {
        o->fields[i].value = v;
        return;
    }
    if (o->count + 1 > o->cap)
    {
        o->cap = o->cap < 8 ? 8 : o->cap * 2;
        o->fields = (Field *)xrealloc(o->fields, sizeof(Field) * (size_t)o->cap);
    }
    o->fields[o->count].key = copy_span(key, (int)strlen(key));
    o->fields[o->count].value = v;
    o->count++;
}

static int value_equal(MzValue a, MzValue b)
{
    if (a.type != b.type)
        return 0;
    if (a.type == MZ_NULL)
        return 1;
    if (a.type == MZ_BOOL)
        return a.as.boolean == b.as.boolean;
    if (a.type == MZ_NUMBER)
        return fabs(a.as.number - b.as.number) < 0.000000001;
    if (a.type == MZ_STRING)
        return !strcmp(((Obj *)a.as.ptr)->as.string.chars, ((Obj *)b.as.ptr)->as.string.chars);
    if (a.type == MZ_NATIVE)
        return a.as.native == b.as.native;
    return a.as.ptr == b.as.ptr;
}

char *mz_to_cstring(MzValue v)
{
    char tmp[128];
    if (v.type == MZ_NULL)
        return copy_span("null", 4);
    if (v.type == MZ_BOOL)
        return v.as.boolean ? copy_span("true", 4) : copy_span("false", 5);
    if (v.type == MZ_NUMBER)
    {
        snprintf(tmp, sizeof(tmp), "%.15g", v.as.number);
        return copy_span(tmp, (int)strlen(tmp));
    }
    if (v.type == MZ_STRING)
    {
        Obj *o = (Obj *)v.as.ptr;
        return copy_span(o->as.string.chars, o->as.string.len);
    }
    if (v.type == MZ_ARRAY)
        return copy_span("[array]", 7);
    if (v.type == MZ_OBJECT)
        return copy_span("{object}", 8);
    if (v.type == MZ_FUNCTION)
        return copy_span("<function>", 10);
    return copy_span("<native>", 8);
}

static Env *env_new(MzVM *vm, Env *parent)
{
    Env *e = (Env *)calloc(1, sizeof(Env));
    if (!e)
        exit(1);
    e->parent = parent;
    e->next = vm->envs;
    vm->envs = e;
    return e;
}

static Binding *env_find_here(Env *e, const char *name)
{
    for (int i = 0; i < e->count; i++)
        if (!strcmp(e->items[i].name, name))
            return &e->items[i];
    return NULL;
}

static Binding *env_find_span(Env *e, const char *s, int n)
{
    for (; e; e = e->parent)
    {
        for (int i = 0; i < e->count; i++)
        {
            if ((int)strlen(e->items[i].name) == n && !memcmp(e->items[i].name, s, (size_t)n))
                return &e->items[i];
        }
    }
    return NULL;
}

static void env_define_span(Env *e, const char *s, int n, MzValue v)
{
    char *name = copy_span(s, n);
    Binding *b = env_find_here(e, name);
    if (b)
    {
        b->value = v;
        free(name);
        return;
    }
    if (e->count + 1 > e->cap)
    {
        e->cap = e->cap < 16 ? 16 : e->cap * 2;
        e->items = (Binding *)xrealloc(e->items, sizeof(Binding) * (size_t)e->cap);
    }
    e->items[e->count].name = name;
    e->items[e->count].value = v;
    e->count++;
}

void mz_define_native(MzVM *vm, const char *name, MzNative fn)
{
    env_define_span(vm->globals, name, (int)strlen(name), native_value(fn));
}

static void fail(MzVM *vm, const char *fmt, ...)
{
    if (vm->failed)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->error, sizeof(vm->error), fmt, ap);
    va_end(ap);
    vm->failed = 1;
}

const char *mz_error(MzVM *vm)
{
    return vm->error;
}

static void mark_value(MzValue v);
static void mark_env(Env *e);

static void mark_obj(Obj *o)
{
    if (!o || o->marked)
        return;
    o->marked = 1;
    if (o->kind == OBJ_ARRAY)
    {
        for (int i = 0; i < o->as.array.count; i++)
            mark_value(o->as.array.items[i]);
    }
    else if (o->kind == OBJ_OBJECT)
    {
        for (int i = 0; i < o->as.object.count; i++)
            mark_value(o->as.object.fields[i].value);
    }
    else if (o->kind == OBJ_FUNCTION)
    {
        mark_env(o->as.function.closure);
    }
}

static void mark_value(MzValue v)
{
    if (v.type == MZ_STRING || v.type == MZ_ARRAY || v.type == MZ_OBJECT || v.type == MZ_FUNCTION)
        mark_obj((Obj *)v.as.ptr);
}

static void mark_env(Env *e)
{
    if (!e || e->marked)
        return;
    e->marked = 1;
    mark_env(e->parent);
    for (int i = 0; i < e->count; i++)
        mark_value(e->items[i].value);
}

static void gc(MzVM *vm, Env *extra)
{
    if (vm->call_depth > 0)
        return;
    mark_env(vm->globals);
    mark_env(extra);
    mark_value(vm->return_value);
    Obj **o = &vm->objects;
    while (*o)
    {
        if ((*o)->marked)
        {
            (*o)->marked = 0;
            o = &(*o)->next;
        }
        else
        {
            Obj *dead = *o;
            *o = dead->next;
            if (dead->kind == OBJ_STRING)
                free(dead->as.string.chars);
            else if (dead->kind == OBJ_ARRAY)
                free(dead->as.array.items);
            else if (dead->kind == OBJ_OBJECT)
            {
                for (int i = 0; i < dead->as.object.count; i++)
                    free(dead->as.object.fields[i].key);
                free(dead->as.object.fields);
            }
            else if (dead->kind == OBJ_FUNCTION)
            {
                free(dead->as.function.name);
                for (int i = 0; i < dead->as.function.arity; i++)
                    free(dead->as.function.params[i]);
                free(dead->as.function.params);
            }
            free(dead);
        }
    }
    Env **e = &vm->envs;
    while (*e)
    {
        if ((*e)->marked)
        {
            (*e)->marked = 0;
            e = &(*e)->next;
        }
        else
        {
            Env *dead = *e;
            *e = dead->next;
            for (int i = 0; i < dead->count; i++)
                free(dead->items[i].name);
            free(dead->items);
            free(dead);
        }
    }
    vm->allocs = 0;
}

static int kw(const char *s, int n, const char *k)
{
    return (int)strlen(k) == n && !memcmp(s, k, (size_t)n);
}

static TokKind keyword(const char *s, int n)
{
    if (kw(s, n, "let") || kw(s, n, "var"))
        return TK_LET;
    if (kw(s, n, "fun") || kw(s, n, "func"))
        return TK_FUN;
    if (kw(s, n, "return"))
        return TK_RETURN;
    if (kw(s, n, "if"))
        return TK_IF;
    if (kw(s, n, "else"))
        return TK_ELSE;
    if (kw(s, n, "while"))
        return TK_WHILE;
    if (kw(s, n, "for"))
        return TK_FOR;
    if (kw(s, n, "switch"))
        return TK_SWITCH;
    if (kw(s, n, "case"))
        return TK_CASE;
    if (kw(s, n, "default"))
        return TK_DEFAULT;
    if (kw(s, n, "break"))
        return TK_BREAK;
    if (kw(s, n, "continue"))
        return TK_CONTINUE;
    if (kw(s, n, "true"))
        return TK_TRUE;
    if (kw(s, n, "false"))
        return TK_FALSE;
    if (kw(s, n, "null"))
        return TK_NULL;
    if (kw(s, n, "and"))
        return TK_AND;
    if (kw(s, n, "or"))
        return TK_OR;
    if (kw(s, n, "not"))
        return TK_NOT;
    return TK_ID;
}

static void tok_push(MzVM *vm, Tok t)
{
    if (vm->token_count + 1 > vm->token_cap)
    {
        vm->token_cap = vm->token_cap < 256 ? 256 : vm->token_cap * 2;
        vm->tokens = (Tok *)xrealloc(vm->tokens, sizeof(Tok) * (size_t)vm->token_cap);
    }
    vm->tokens[vm->token_count++] = t;
}

static int lex(MzVM *vm, const char *src)
{
    int i = 0, line = 1;
    while (src[i])
    {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\r')
        {
            i++;
            continue;
        }
        if (c == '\n')
        {
            line++;
            i++;
            continue;
        }
        if (c == '#' || (c == '/' && src[i + 1] == '/'))
        {
            while (src[i] && src[i] != '\n')
                i++;
            continue;
        }
        if (c == '/' && src[i + 1] == '*')
        {
            i += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/'))
            {
                if (src[i] == '\n')
                    line++;
                i++;
            }
            if (!src[i])
            {
                fail(vm, "comentario sem fechamento");
                return 0;
            }
            i += 2;
            continue;
        }
        Tok t;
        memset(&t, 0, sizeof(t));
        t.s = src + i;
        t.line = line;
        t.len = 1;
        if (isalpha((unsigned char)c) || c == '_')
        {
            int st = i;
            while (isalnum((unsigned char)src[i]) || src[i] == '_')
                i++;
            t.s = src + st;
            t.len = i - st;
            t.kind = keyword(t.s, t.len);
            tok_push(vm, t);
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)src[i + 1])))
        {
            int st = i;
            while (isdigit((unsigned char)src[i]))
                i++;
            if (src[i] == '.')
            {
                i++;
                while (isdigit((unsigned char)src[i]))
                    i++;
            }
            t.s = src + st;
            t.len = i - st;
            t.kind = TK_NUM;
            t.number = strtod(t.s, NULL);
            tok_push(vm, t);
            continue;
        }
        if (c == '"')
        {
            int st = ++i;
            while (src[i] && src[i] != '"')
            {
                if (src[i] == '\\' && src[i + 1])
                    i++;
                i++;
            }
            if (!src[i])
            {
                fail(vm, "string sem fechamento");
                return 0;
            }
            t.kind = TK_STR;
            t.s = src + st;
            t.len = i - st;
            i++;
            tok_push(vm, t);
            continue;
        }
        switch (c)
        {
        case '+':
            if (src[i + 1] == '=')
            {
                t.kind = TK_PLUSEQ;
                t.len = 2;
                i++;
            }
            else if (src[i + 1] == '+')
            {
                t.kind = TK_PLUSPLUS;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_PLUS;
            break;
        case '-':
            if (src[i + 1] == '=')
            {
                t.kind = TK_MINUSEQ;
                t.len = 2;
                i++;
            }
            else if (src[i + 1] == '-')
            {
                t.kind = TK_MINUSMINUS;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_MINUS;
            break;
        case '*':
            if (src[i + 1] == '=')
            {
                t.kind = TK_STAREQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_STAR;
            break;
        case '/':
            if (src[i + 1] == '=')
            {
                t.kind = TK_SLASHEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_SLASH;
            break;
        case '%':
            if (src[i + 1] == '=')
            {
                t.kind = TK_PERCENTEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_PERCENT;
            break;
        case '=':
            if (src[i + 1] == '=')
            {
                t.kind = TK_EQEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_EQ;
            break;
        case '!':
            if (src[i + 1] == '=')
            {
                t.kind = TK_BANGEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_BANG;
            break;
        case '<':
            if (src[i + 1] == '=')
            {
                t.kind = TK_LTEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_LT;
            break;
        case '>':
            if (src[i + 1] == '=')
            {
                t.kind = TK_GTEQ;
                t.len = 2;
                i++;
            }
            else
                t.kind = TK_GT;
            break;
        case '&':
            if (src[i + 1] == '&')
            {
                t.kind = TK_AND;
                t.len = 2;
                i++;
            }
            else
            {
                fail(vm, "caractere '&' inesperado");
                return 0;
            }
            break;
        case '|':
            if (src[i + 1] == '|')
            {
                t.kind = TK_OR;
                t.len = 2;
                i++;
            }
            else
            {
                fail(vm, "caractere '|' inesperado");
                return 0;
            }
            break;
        case '(':
            t.kind = TK_LP;
            break;
        case ')':
            t.kind = TK_RP;
            break;
        case '[':
            t.kind = TK_LB;
            break;
        case ']':
            t.kind = TK_RB;
            break;
        case '{':
            t.kind = TK_LC;
            break;
        case '}':
            t.kind = TK_RC;
            break;
        case ',':
            t.kind = TK_COMMA;
            break;
        case '.':
            t.kind = TK_DOT;
            break;
        case ':':
            t.kind = TK_COLON;
            break;
        case ';':
            t.kind = TK_SEMI;
            break;
        default:
            fail(vm, "linha %d: caractere inesperado '%c'", line, c);
            return 0;
        }
        i++;
        tok_push(vm, t);
    }
    Tok eof;
    memset(&eof, 0, sizeof(eof));
    eof.kind = TK_EOF;
    eof.s = src + i;
    eof.line = line;
    tok_push(vm, eof);
    return 1;
}

static Tok *peek(Parser *p) { return &p->vm->tokens[p->pos]; }
static Tok *prev(Parser *p) { return &p->vm->tokens[p->pos - 1]; }
static int check(Parser *p, TokKind k) { return peek(p)->kind == k; }
static int at_end(Parser *p) { return p->pos >= p->end || check(p, TK_EOF); }
static int match(Parser *p, TokKind k)
{
    if (!check(p, k))
        return 0;
    p->pos++;
    return 1;
}
static const char *tok_name(TokKind k)
{
    switch (k)
    {
    case TK_EOF:
        return "fim do arquivo";
    case TK_ID:
        return "identificador";
    case TK_NUM:
        return "numero";
    case TK_STR:
        return "string";
    case TK_LET:
        return "let";
    case TK_FUN:
        return "fun";
    case TK_RETURN:
        return "return";
    case TK_IF:
        return "if";
    case TK_ELSE:
        return "else";
    case TK_WHILE:
        return "while";
    case TK_FOR:
        return "for";
    case TK_SWITCH:
        return "switch";
    case TK_CASE:
        return "case";
    case TK_DEFAULT:
        return "default";
    case TK_BREAK:
        return "break";
    case TK_CONTINUE:
        return "continue";
    case TK_TRUE:
        return "true";
    case TK_FALSE:
        return "false";
    case TK_NULL:
        return "null";
    case TK_AND:
        return "and";
    case TK_OR:
        return "or";
    case TK_NOT:
        return "not";
    case TK_PLUS:
        return "+";
    case TK_PLUSPLUS:
        return "++";
    case TK_MINUS:
        return "-";
    case TK_MINUSMINUS:
        return "--";
    case TK_STAR:
        return "*";
    case TK_SLASH:
        return "/";
    case TK_PERCENT:
        return "%";
    case TK_EQ:
        return "=";
    case TK_PLUSEQ:
        return "+=";
    case TK_MINUSEQ:
        return "-=";
    case TK_STAREQ:
        return "*=";
    case TK_SLASHEQ:
        return "/=";
    case TK_PERCENTEQ:
        return "%=";
    case TK_EQEQ:
        return "==";
    case TK_BANG:
        return "!";
    case TK_BANGEQ:
        return "!=";
    case TK_LT:
        return "<";
    case TK_LTEQ:
        return "<=";
    case TK_GT:
        return ">";
    case TK_GTEQ:
        return ">=";
    case TK_LP:
        return "(";
    case TK_RP:
        return ")";
    case TK_LB:
        return "[";
    case TK_RB:
        return "]";
    case TK_LC:
        return "{";
    case TK_RC:
        return "}";
    case TK_COMMA:
        return ",";
    case TK_DOT:
        return ".";
    case TK_COLON:
        return ":";
    case TK_SEMI:
        return ";";
    }
    return "token";
}
static Tok *consume(Parser *p, TokKind k, const char *msg)
{
    if (check(p, k))
        return &p->vm->tokens[p->pos++];
    Tok *t = peek(p);
    if (t->kind == TK_ID || t->kind == TK_NUM || t->kind == TK_STR)
    {
        fail(p->vm, "linha %d: %s antes de '%.*s'", t->line, msg, t->len, t->s);
    }
    else
    {
        fail(p->vm, "linha %d: %s antes de %s", t->line, msg, tok_name(t->kind));
    }
    return peek(p);
}

static MzValue primary(Parser *p);

static MzFunction *as_func(MzValue v) { return &((Obj *)v.as.ptr)->as.function; }
static MzArray *as_array(MzValue v) { return &((Obj *)v.as.ptr)->as.array; }
static MzObject *as_object(MzValue v) { return &((Obj *)v.as.ptr)->as.object; }
static const char *as_string(MzValue v) { return ((Obj *)v.as.ptr)->as.string.chars; }

static int block_end(Parser *p, int open)
{
    int depth = 0;
    for (int i = open; i < p->end; i++)
    {
        if (p->vm->tokens[i].kind == TK_LC)
            depth++;
        else if (p->vm->tokens[i].kind == TK_RC)
        {
            depth--;
            if (!depth)
                return i;
        }
    }
    fail(p->vm, "bloco sem fechamento");
    return open;
}

static MzValue parse_function(Parser *p, const char *name)
{
    consume(p, TK_LP, "esperado '('");
    char *params[64];
    int arity = 0;
    if (!check(p, TK_RP))
    {
        do
        {
            Tok *id = consume(p, TK_ID, "esperado parametro");
            if (arity >= 64)
            {
                fail(p->vm, "muitos parametros");
                return mz_null();
            }
            params[arity++] = copy_span(id->s, id->len);
        } while (match(p, TK_COMMA));
    }
    consume(p, TK_RP, "esperado ')'");
    Tok *open = consume(p, TK_LC, "esperado corpo da funcao");
    int open_i = (int)(open - p->vm->tokens);
    int close_i = block_end(p, open_i);
    MzValue fv = function_new(p->vm, name ? name : "<anon>", arity);
    MzFunction *fn = as_func(fv);
    for (int i = 0; i < arity; i++)
        fn->params[i] = params[i];
    fn->body_start = open_i + 1;
    fn->body_end = close_i;
    fn->closure = p->env;
    p->pos = close_i + 1;
    return fv;
}

static MzValue primary(Parser *p)
{
    if (match(p, TK_NUM))
        return mz_number(prev(p)->number);
    if (match(p, TK_STR))
        return string_take(p->vm, copy_span(prev(p)->s, prev(p)->len));
    if (match(p, TK_TRUE))
        return mz_bool(1);
    if (match(p, TK_FALSE))
        return mz_bool(0);
    if (match(p, TK_NULL))
        return mz_null();
    if (match(p, TK_FUN))
        return parse_function(p, NULL);
    if (match(p, TK_ID))
    {
        Tok *id = prev(p);
        Binding *b = env_find_span(p->env, id->s, id->len);
        if (!b)
        {
            fail(p->vm, "linha %d: nome '%.*s' nao definido", id->line, id->len, id->s);
            return mz_null();
        }
        return b->value;
    }
    if (match(p, TK_LP))
    {
        MzValue v = expression(p);
        consume(p, TK_RP, "esperado ')'");
        return v;
    }
    if (match(p, TK_LB))
    {
        MzValue av = array_new(p->vm);
        if (!check(p, TK_RB))
        {
            do
            {
                array_push(as_array(av), expression(p));
            } while (match(p, TK_COMMA));
        }
        consume(p, TK_RB, "esperado ']'");
        return av;
    }
    if (match(p, TK_LC))
    {
        MzValue ov = object_new(p->vm);
        if (!check(p, TK_RC))
        {
            do
            {
                char *key = NULL;
                if (match(p, TK_ID) || match(p, TK_STR))
                    key = copy_span(prev(p)->s, prev(p)->len);
                else
                {
                    fail(p->vm, "esperado chave do objeto");
                    return ov;
                }
                consume(p, TK_COLON, "esperado ':'");
                object_set(as_object(ov), key, expression(p));
                free(key);
            } while (match(p, TK_COMMA));
        }
        consume(p, TK_RC, "esperado '}'");
        return ov;
    }
    fail(p->vm, "linha %d: expressao esperada", peek(p)->line);
    return mz_null();
}

static MzValue postfix(Parser *p)
{
    MzValue v = primary(p);
    MzValue receiver = mz_null();
    int has_receiver = 0;
    while (!p->vm->failed)
    {
        if (match(p, TK_LP))
        {
            MzValue args[64];
            int argc = 0;
            if (!check(p, TK_RP))
            {
                do
                {
                    if (argc >= 64)
                    {
                        fail(p->vm, "muitos argumentos");
                        return mz_null();
                    }
                    args[argc++] = expression(p);
                } while (match(p, TK_COMMA));
            }
            consume(p, TK_RP, "esperado ')'");
            v = call_value(p->vm, p->env, v, argc, args, receiver, has_receiver);
            has_receiver = 0;
        }
        else if (match(p, TK_LB))
        {
            MzValue idx = expression(p);
            consume(p, TK_RB, "esperado ']'");
            if (v.type == MZ_ARRAY)
                v = array_get(as_array(v), (int)mz_as_number(idx));
            else if (v.type == MZ_OBJECT)
            {
                char *key = mz_to_cstring(idx);
                v = object_get(as_object(v), key);
                free(key);
            }
            else if (v.type == MZ_STRING && idx.type == MZ_NUMBER)
            {
                const char *s = as_string(v);
                int i = (int)idx.as.number;
                if (i < 0 || i >= (int)strlen(s))
                    v = mz_null();
                else
                {
                    char ch[2] = {s[i], 0};
                    v = mz_string(p->vm, ch);
                }
            }
            else
                v = mz_null();
        }
        else if (match(p, TK_DOT))
        {
            Tok *field = consume(p, TK_ID, "esperado campo");
            receiver = v;
            has_receiver = 1;
            if (v.type == MZ_OBJECT)
            {
                char *key = copy_span(field->s, field->len);
                v = object_get(as_object(v), key);
                free(key);
            }
            else
                v = mz_null();
        }
        else
            break;
    }
    return v;
}

static MzValue unary(Parser *p)
{
    if (match(p, TK_MINUS))
        return mz_number(-mz_as_number(unary(p)));
    if (match(p, TK_BANG) || match(p, TK_NOT))
        return mz_bool(!mz_truthy(unary(p)));
    return postfix(p);
}

static MzValue factor(Parser *p)
{
    MzValue a = unary(p);
    while (check(p, TK_STAR) || check(p, TK_SLASH) || check(p, TK_PERCENT))
    {
        TokKind op = peek(p)->kind;
        p->pos++;
        MzValue b = unary(p);
        if (op == TK_STAR)
            a = mz_number(mz_as_number(a) * mz_as_number(b));
        else if (op == TK_SLASH)
            a = mz_number(mz_as_number(a) / mz_as_number(b));
        else
            a = mz_number(fmod(mz_as_number(a), mz_as_number(b)));
    }
    return a;
}

static MzValue term(Parser *p)
{
    MzValue a = factor(p);
    while (check(p, TK_PLUS) || check(p, TK_MINUS))
    {
        TokKind op = peek(p)->kind;
        p->pos++;
        MzValue b = factor(p);
        if (op == TK_PLUS && (a.type == MZ_STRING || b.type == MZ_STRING))
        {
            char *sa = mz_to_cstring(a), *sb = mz_to_cstring(b);
            int na = (int)strlen(sa), nb = (int)strlen(sb);
            char *out = (char *)malloc((size_t)na + (size_t)nb + 1);
            memcpy(out, sa, (size_t)na);
            memcpy(out + na, sb, (size_t)nb + 1);
            free(sa);
            free(sb);
            a = string_take(p->vm, out);
        }
        else if (op == TK_PLUS)
            a = mz_number(mz_as_number(a) + mz_as_number(b));
        else
            a = mz_number(mz_as_number(a) - mz_as_number(b));
    }
    return a;
}

static MzValue compare(Parser *p)
{
    MzValue a = term(p);
    while (check(p, TK_LT) || check(p, TK_LTEQ) || check(p, TK_GT) || check(p, TK_GTEQ))
    {
        TokKind op = peek(p)->kind;
        p->pos++;
        MzValue b = term(p);
        if (op == TK_LT)
            a = mz_bool(mz_as_number(a) < mz_as_number(b));
        else if (op == TK_LTEQ)
            a = mz_bool(mz_as_number(a) <= mz_as_number(b));
        else if (op == TK_GT)
            a = mz_bool(mz_as_number(a) > mz_as_number(b));
        else
            a = mz_bool(mz_as_number(a) >= mz_as_number(b));
    }
    return a;
}

static MzValue equality(Parser *p)
{
    MzValue a = compare(p);
    while (check(p, TK_EQEQ) || check(p, TK_BANGEQ))
    {
        TokKind op = peek(p)->kind;
        p->pos++;
        MzValue b = compare(p);
        int ok = value_equal(a, b);
        a = mz_bool(op == TK_EQEQ ? ok : !ok);
    }
    return a;
}

static MzValue logic_and(Parser *p)
{
    MzValue a = equality(p);
    while (match(p, TK_AND))
    {
        MzValue b = equality(p);
        a = mz_bool(mz_truthy(a) && mz_truthy(b));
    }
    return a;
}

static MzValue expression(Parser *p)
{
    MzValue a = logic_and(p);
    while (match(p, TK_OR))
    {
        MzValue b = logic_and(p);
        a = mz_bool(mz_truthy(a) || mz_truthy(b));
    }
    return a;
}

static int parse_lvalue(Parser *p, LValue *lv)
{
    memset(lv, 0, sizeof(*lv));
    if (!check(p, TK_ID))
        return 0;
    Tok *id = consume(p, TK_ID, "esperado nome");
    lv->type = 1;
    lv->name = copy_span(id->s, id->len);
    Binding *b = env_find_span(p->env, id->s, id->len);
    MzValue cur = b ? b->value : mz_null();
    while (check(p, TK_DOT) || check(p, TK_LB))
    {
        if (match(p, TK_DOT))
        {
            Tok *field = consume(p, TK_ID, "esperado campo");
            free(lv->name);
            lv->type = 3;
            lv->base = cur;
            lv->name = copy_span(field->s, field->len);
            cur = cur.type == MZ_OBJECT ? object_get(as_object(cur), lv->name) : mz_null();
        }
        else
        {
            match(p, TK_LB);
            MzValue idx = expression(p);
            consume(p, TK_RB, "esperado ']'");
            lv->type = 2;
            lv->base = cur;
            lv->index = idx;
            if (cur.type == MZ_ARRAY)
                cur = array_get(as_array(cur), (int)mz_as_number(idx));
            else if (cur.type == MZ_OBJECT)
            {
                char *key = mz_to_cstring(idx);
                cur = object_get(as_object(cur), key);
                free(key);
            }
            else
                cur = mz_null();
        }
    }
    lv->current = cur;
    return 1;
}

static void assign_lvalue(Parser *p, LValue *lv, MzValue v)
{
    if (lv->type == 1)
    {
        Binding *b = env_find_span(p->env, lv->name, (int)strlen(lv->name));
        if (b)
            b->value = v;
        else
            env_define_span(p->env, lv->name, (int)strlen(lv->name), v);
    }
    else if (lv->type == 2)
    {
        if (lv->base.type == MZ_ARRAY)
            array_set(as_array(lv->base), (int)mz_as_number(lv->index), v);
        else if (lv->base.type == MZ_OBJECT)
        {
            char *key = mz_to_cstring(lv->index);
            object_set(as_object(lv->base), key, v);
            free(key);
        }
    }
    else if (lv->type == 3 && lv->base.type == MZ_OBJECT)
    {
        object_set(as_object(lv->base), lv->name, v);
    }
}

static void free_lvalue(LValue *lv) { free(lv->name); }

static int next_assignment(Parser *p)
{
    int old = p->pos;
    LValue lv;
    int ok = parse_lvalue(p, &lv);
    free_lvalue(&lv);
    int assign = ok && (check(p, TK_EQ) || check(p, TK_PLUSEQ) || check(p, TK_MINUSEQ) || check(p, TK_STAREQ) || check(p, TK_SLASHEQ) || check(p, TK_PERCENTEQ) || check(p, TK_PLUSPLUS) || check(p, TK_MINUSMINUS));
    p->pos = old;
    return assign;
}

static void skip_statement(Parser *p)
{
    if (match(p, TK_LC))
    {
        p->pos = block_end(p, p->pos - 1) + 1;
        return;
    }
    if (match(p, TK_IF))
    {
        int depth = 0;
        do
        {
            if (check(p, TK_LP))
                depth++;
            else if (check(p, TK_RP))
                depth--;
            p->pos++;
        } while (!at_end(p) && depth > 0);
        skip_statement(p);
        if (match(p, TK_ELSE))
            skip_statement(p);
        return;
    }
    if (match(p, TK_WHILE) || match(p, TK_FOR))
    {
        if (match(p, TK_LP))
        {
            int depth = 1;
            while (!at_end(p) && depth > 0)
            {
                if (check(p, TK_LP))
                    depth++;
                else if (check(p, TK_RP))
                    depth--;
                p->pos++;
            }
        }
        skip_statement(p);
        return;
    }
    if (match(p, TK_SWITCH))
    {
        if (match(p, TK_LP))
        {
            int depth = 1;
            while (!at_end(p) && depth > 0)
            {
                if (check(p, TK_LP))
                    depth++;
                else if (check(p, TK_RP))
                    depth--;
                p->pos++;
            }
        }
        if (match(p, TK_LC))
            p->pos = block_end(p, p->pos - 1) + 1;
        return;
    }
    while (!at_end(p) && !check(p, TK_SEMI) && !check(p, TK_RC))
        p->pos++;
    match(p, TK_SEMI);
}

static void block(Parser *p, Env *env)
{
    consume(p, TK_LC, "esperado '{'");
    Env *old = p->env;
    p->env = env;
    while (!check(p, TK_RC) && !at_end(p) && !p->vm->failed && !p->vm->returning && !p->vm->breaking && !p->vm->continuing)
        statement(p);
    consume(p, TK_RC, "esperado '}'");
    p->env = old;
}

static void var_statement(Parser *p)
{
    Tok *id = consume(p, TK_ID, "esperado nome");
    MzValue v = mz_null();
    if (match(p, TK_EQ))
        v = expression(p);
    env_define_span(p->env, id->s, id->len, v);
    match(p, TK_SEMI);
}

static void function_statement(Parser *p)
{
    Tok *name = consume(p, TK_ID, "esperado nome da funcao");
    char *s = copy_span(name->s, name->len);
    MzValue fv = parse_function(p, s);
    env_define_span(p->env, name->s, name->len, fv);
    free(s);
}

static void if_statement(Parser *p)
{
    consume(p, TK_LP, "esperado '('");
    MzValue cond = expression(p);
    consume(p, TK_RP, "esperado ')'");
    if (mz_truthy(cond))
    {
        statement(p);
        if (match(p, TK_ELSE))
            skip_statement(p);
    }
    else
    {
        skip_statement(p);
        if (match(p, TK_ELSE))
            statement(p);
    }
}

static void while_statement(Parser *p)
{
    consume(p, TK_LP, "esperado '('");
    int cond_start = p->pos;
    int depth = 1;
    while (!at_end(p) && depth > 0)
    {
        if (check(p, TK_LP))
            depth++;
        else if (check(p, TK_RP))
        {
            depth--;
            if (depth == 0)
                break;
        }
        p->pos++;
    }
    int cond_end = p->pos;
    consume(p, TK_RP, "esperado ')'");
    Tok *open = consume(p, TK_LC, "while precisa de bloco");
    int open_i = (int)(open - p->vm->tokens);
    int close_i = block_end(p, open_i);
    while (!p->vm->failed)
    {
        Parser cp = *p;
        cp.pos = cond_start;
        cp.end = cond_end;
        if (!mz_truthy(expression(&cp)))
            break;
        Parser body = *p;
        body.pos = open_i + 1;
        body.end = close_i;
        body.env = env_new(p->vm, p->env);
        while (!at_end(&body) && !p->vm->failed && !p->vm->returning && !p->vm->breaking && !p->vm->continuing)
            statement(&body);
        if (p->vm->returning)
            break;
        if (p->vm->breaking)
        {
            p->vm->breaking = 0;
            break;
        }
        if (p->vm->continuing)
            p->vm->continuing = 0;
        if (p->vm->allocs > MZ_GC_THRESHOLD)
            gc(p->vm, body.env);
    }
    p->pos = close_i + 1;
}

static void range_statement(Parser *p, int start, int end, Env *env)
{
    Parser sub = *p;
    sub.pos = start;
    sub.end = end;
    sub.env = env;
    if (match(&sub, TK_LET))
        var_statement(&sub);
    else
        expr_statement(&sub);
}

static MzValue range_expression(Parser *p, int start, int end, Env *env)
{
    Parser sub = *p;
    sub.pos = start;
    sub.end = end;
    sub.env = env;
    if (start >= end)
        return mz_bool(1);
    return expression(&sub);
}

static void for_statement(Parser *p)
{
    consume(p, TK_LP, "esperado '('");
    int init_start = p->pos;
    while (!at_end(p) && !check(p, TK_SEMI))
        p->pos++;
    int init_end = p->pos;
    consume(p, TK_SEMI, "esperado ';'");
    int cond_start = p->pos;
    while (!at_end(p) && !check(p, TK_SEMI))
        p->pos++;
    int cond_end = p->pos;
    consume(p, TK_SEMI, "esperado ';'");
    int inc_start = p->pos;
    int depth = 1;
    while (!at_end(p) && depth > 0)
    {
        if (check(p, TK_LP))
            depth++;
        else if (check(p, TK_RP))
        {
            depth--;
            if (depth == 0)
                break;
        }
        p->pos++;
    }
    int inc_end = p->pos;
    consume(p, TK_RP, "esperado ')'");
    Tok *open = consume(p, TK_LC, "for precisa de bloco");
    int open_i = (int)(open - p->vm->tokens);
    int close_i = block_end(p, open_i);
    Env *loop_env = env_new(p->vm, p->env);
    if (init_start < init_end)
        range_statement(p, init_start, init_end, loop_env);
    while (!p->vm->failed && mz_truthy(range_expression(p, cond_start, cond_end, loop_env)))
    {
        Parser body = *p;
        body.pos = open_i + 1;
        body.end = close_i;
        body.env = loop_env;
        while (!at_end(&body) && !p->vm->failed && !p->vm->returning && !p->vm->breaking && !p->vm->continuing)
            statement(&body);
        if (p->vm->returning)
            break;
        if (p->vm->breaking)
        {
            p->vm->breaking = 0;
            break;
        }
        if (p->vm->continuing)
            p->vm->continuing = 0;
        if (inc_start < inc_end)
            range_statement(p, inc_start, inc_end, loop_env);
        if (p->vm->allocs > MZ_GC_THRESHOLD)
            gc(p->vm, loop_env);
    }
    p->pos = close_i + 1;
}

static int switch_case_colon(Parser *p, int start, int end)
{
    int par = 0, br = 0, obj = 0;
    for (int i = start; i < end; i++)
    {
        TokKind k = p->vm->tokens[i].kind;
        if (k == TK_LP)
            par++;
        else if (k == TK_RP)
            par--;
        else if (k == TK_LB)
            br++;
        else if (k == TK_RB)
            br--;
        else if (k == TK_LC)
            obj++;
        else if (k == TK_RC)
            obj--;
        else if (k == TK_COLON && par == 0 && br == 0 && obj == 0)
            return i;
    }
    fail(p->vm, "linha %d: esperado ':' no case", p->vm->tokens[start - 1].line);
    return start;
}

static int switch_next_label(Parser *p, int start, int end)
{
    int depth = 0;
    for (int i = start; i < end; i++)
    {
        TokKind k = p->vm->tokens[i].kind;
        if (k == TK_LC)
            depth++;
        else if (k == TK_RC)
            depth--;
        else if (depth == 0 && (k == TK_CASE || k == TK_DEFAULT))
            return i;
    }
    return end;
}

static void switch_statement(Parser *p)
{
    consume(p, TK_LP, "esperado '('");
    MzValue target = expression(p);
    consume(p, TK_RP, "esperado ')'");
    Tok *open = consume(p, TK_LC, "switch precisa de bloco");
    int open_i = (int)(open - p->vm->tokens);
    int close_i = block_end(p, open_i);
    int selected_start = -1, selected_end = -1;
    int default_start = -1, default_end = -1;

    for (int i = open_i + 1; i < close_i && !p->vm->failed;)
    {
        TokKind k = p->vm->tokens[i].kind;
        if (k == TK_CASE)
        {
            int colon = switch_case_colon(p, i + 1, close_i);
            int next = switch_next_label(p, colon + 1, close_i);
            Parser cp = *p;
            cp.pos = i + 1;
            cp.end = colon;
            if (selected_start < 0 && value_equal(target, expression(&cp)))
            {
                selected_start = colon + 1;
                selected_end = next;
            }
            i = next;
        }
        else if (k == TK_DEFAULT)
        {
            int colon = switch_case_colon(p, i + 1, close_i);
            int next = switch_next_label(p, colon + 1, close_i);
            default_start = colon + 1;
            default_end = next;
            i = next;
        }
        else
        {
            fail(p->vm, "linha %d: esperado case ou default dentro do switch", p->vm->tokens[i].line);
        }
    }

    if (selected_start < 0)
    {
        selected_start = default_start;
        selected_end = default_end;
    }
    if (selected_start >= 0 && !p->vm->failed)
    {
        Parser body = *p;
        body.pos = selected_start;
        body.end = selected_end;
        body.env = env_new(p->vm, p->env);
        while (!at_end(&body) && !p->vm->failed && !p->vm->returning && !p->vm->breaking && !p->vm->continuing)
            statement(&body);
        if (p->vm->breaking)
            p->vm->breaking = 0;
    }
    p->pos = close_i + 1;
}

static void return_statement(Parser *p)
{
    MzValue v = check(p, TK_SEMI) || check(p, TK_RC) ? mz_null() : expression(p);
    p->vm->return_value = v;
    p->vm->returning = 1;
    match(p, TK_SEMI);
}

static void expr_statement(Parser *p)
{
    if (next_assignment(p))
    {
        LValue lv;
        parse_lvalue(p, &lv);
        TokKind op = peek(p)->kind;
        p->pos++;
        MzValue v;
        if (op == TK_PLUSPLUS || op == TK_MINUSMINUS)
        {
            v = mz_number(mz_as_number(lv.current) + (op == TK_PLUSPLUS ? 1 : -1));
        }
        else
        {
            v = expression(p);
            if (op != TK_EQ)
            {
                double a = mz_as_number(lv.current), b = mz_as_number(v);
                if (op == TK_PLUSEQ)
                    v = mz_number(a + b);
                else if (op == TK_MINUSEQ)
                    v = mz_number(a - b);
                else if (op == TK_STAREQ)
                    v = mz_number(a * b);
                else if (op == TK_SLASHEQ)
                    v = mz_number(a / b);
                else
                    v = mz_number(fmod(a, b));
            }
        }
        assign_lvalue(p, &lv, v);
        free_lvalue(&lv);
    }
    else
    {
        (void)expression(p);
    }
    match(p, TK_SEMI);
}

static void statement(Parser *p)
{
    if (p->vm->failed || p->vm->returning || p->vm->breaking || p->vm->continuing)
        return;
    if (match(p, TK_SEMI))
        return;
    if (match(p, TK_LET))
        var_statement(p);
    else if (match(p, TK_FUN))
        function_statement(p);
    else if (match(p, TK_IF))
        if_statement(p);
    else if (match(p, TK_WHILE))
        while_statement(p);
    else if (match(p, TK_FOR))
        for_statement(p);
    else if (match(p, TK_SWITCH))
        switch_statement(p);
    else if (match(p, TK_RETURN))
        return_statement(p);
    else if (match(p, TK_BREAK))
    {
        p->vm->breaking = 1;
        match(p, TK_SEMI);
    }
    else if (match(p, TK_CONTINUE))
    {
        p->vm->continuing = 1;
        match(p, TK_SEMI);
    }
    else if (match(p, TK_LC))
    {
        p->pos--;
        block(p, env_new(p->vm, p->env));
    }
    else
        expr_statement(p);
    if (p->env == p->vm->globals && p->vm->allocs > MZ_GC_THRESHOLD)
        gc(p->vm, p->env);
}

static MzValue call_value(MzVM *vm, Env *fallback, MzValue callee, int argc, MzValue *argv, MzValue thisv, int has_this)
{
    if (callee.type == MZ_NATIVE)
        return callee.as.native(vm, argc, argv);
    if (callee.type != MZ_FUNCTION)
    {
        fail(vm, "valor nao chamavel");
        return mz_null();
    }
    MzFunction *fn = as_func(callee);
    if (argc < fn->arity)
    {
        fail(vm, "funcao '%s' esperava %d argumentos", fn->name, fn->arity);
        return mz_null();
    }
    Env *call_env = env_new(vm, fn->closure ? fn->closure : fallback);
    if (has_this)
        env_define_span(call_env, "this", 4, thisv);
    for (int i = 0; i < fn->arity; i++)
        env_define_span(call_env, fn->params[i], (int)strlen(fn->params[i]), argv[i]);
    Parser body;
    body.vm = vm;
    body.pos = fn->body_start;
    body.end = fn->body_end;
    body.env = call_env;
    int old_ret = vm->returning, old_br = vm->breaking, old_ct = vm->continuing;
    MzValue old_val = vm->return_value;
    vm->returning = vm->breaking = vm->continuing = 0;
    vm->return_value = mz_null();
    vm->call_depth++;
    while (!at_end(&body) && !vm->failed && !vm->returning)
        statement(&body);
    vm->call_depth--;
    MzValue result = vm->returning ? vm->return_value : mz_null();
    vm->returning = old_ret;
    vm->breaking = old_br;
    vm->continuing = old_ct;
    vm->return_value = old_val;
    return result;
}

static MzValue n_print(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    for (int i = 0; i < argc; i++)
    {
        if (i)
            putchar(' ');
        char *s = mz_to_cstring(argv[i]);
        fputs(s, stdout);
        free(s);
    }
    putchar('\n');
    return mz_null();
}

static MzValue n_len(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_number(0);
    if (argv[0].type == MZ_STRING)
        return mz_number(((Obj *)argv[0].as.ptr)->as.string.len);
    if (argv[0].type == MZ_ARRAY)
        return mz_number(as_array(argv[0])->count);
    if (argv[0].type == MZ_OBJECT)
        return mz_number(as_object(argv[0])->count);
    return mz_number(0);
}

static MzValue n_str(MzVM *vm, int argc, MzValue *argv) { return argc < 1 ? mz_string(vm, "") : string_take(vm, mz_to_cstring(argv[0])); }
static MzValue n_num(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    return argc < 1 ? mz_number(0) : mz_number(mz_as_number(argv[0]));
}
static MzValue n_type(MzVM *vm, int argc, MzValue *argv)
{
    if (argc < 1 || argv[0].type == MZ_NULL)
        return mz_string(vm, "null");
    if (argv[0].type == MZ_NUMBER)
        return mz_string(vm, "number");
    if (argv[0].type == MZ_BOOL)
        return mz_string(vm, "bool");
    if (argv[0].type == MZ_STRING)
        return mz_string(vm, "string");
    if (argv[0].type == MZ_ARRAY)
        return mz_string(vm, "array");
    if (argv[0].type == MZ_OBJECT)
        return mz_string(vm, "object");
    if (argv[0].type == MZ_FUNCTION)
        return mz_string(vm, "function");
    return mz_string(vm, "native");
}

static MzValue n_push(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc >= 2 && argv[0].type == MZ_ARRAY)
    {
        array_push(as_array(argv[0]), argv[1]);
        return mz_number(as_array(argv[0])->count);
    }
    return mz_null();
}
static MzValue n_pop(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc >= 1 && argv[0].type == MZ_ARRAY && as_array(argv[0])->count > 0)
        return as_array(argv[0])->items[--as_array(argv[0])->count];
    return mz_null();
}
static MzValue n_remove(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 2 || argv[0].type != MZ_ARRAY)
        return mz_null();
    MzArray *a = as_array(argv[0]);
    int idx = (int)mz_as_number(argv[1]);
    if (idx < 0 || idx >= a->count)
        return mz_null();
    MzValue v = a->items[idx];
    for (int i = idx; i < a->count - 1; i++)
        a->items[i] = a->items[i + 1];
    a->count--;
    return v;
}

static MzValue n_has(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 2)
        return mz_bool(0);
    if (argv[0].type == MZ_OBJECT)
    {
        char *k = mz_to_cstring(argv[1]);
        int ok = field_find(as_object(argv[0]), k) >= 0;
        free(k);
        return mz_bool(ok);
    }
    if (argv[0].type == MZ_ARRAY)
    {
        int i = (int)mz_as_number(argv[1]);
        return mz_bool(i >= 0 && i < as_array(argv[0])->count);
    }
    return mz_bool(0);
}

static MzValue n_del(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 2 || argv[0].type != MZ_OBJECT)
        return mz_bool(0);
    char *k = mz_to_cstring(argv[1]);
    MzObject *o = as_object(argv[0]);
    int idx = field_find(o, k);
    free(k);
    if (idx < 0)
        return mz_bool(0);
    free(o->fields[idx].key);
    for (int i = idx; i < o->count - 1; i++)
        o->fields[i] = o->fields[i + 1];
    o->count--;
    return mz_bool(1);
}

static MzValue n_range(MzVM *vm, int argc, MzValue *argv)
{
    double start = 0, end = argc ? mz_as_number(argv[0]) : 0, step = 1;
    if (argc >= 2)
    {
        start = mz_as_number(argv[0]);
        end = mz_as_number(argv[1]);
    }
    if (argc >= 3)
        step = mz_as_number(argv[2]);
    if (step == 0)
        step = 1;
    MzValue a = array_new(vm);
    if (step > 0)
        for (double x = start; x < end; x += step)
            array_push(as_array(a), mz_number(x));
    else
        for (double x = start; x > end; x += step)
            array_push(as_array(a), mz_number(x));
    return a;
}

static MzValue n_join(MzVM *vm, int argc, MzValue *argv)
{
    if (argc < 1 || argv[0].type != MZ_ARRAY)
        return mz_string(vm, "");
    const char *sep = argc >= 2 && argv[1].type == MZ_STRING ? as_string(argv[1]) : "";
    int cap = 64, len = 0;
    char *out = (char *)malloc((size_t)cap);
    out[0] = 0;
    MzArray *a = as_array(argv[0]);
    for (int i = 0; i < a->count; i++)
    {
        char *s = mz_to_cstring(a->items[i]);
        int need = (int)strlen(s) + (i ? (int)strlen(sep) : 0);
        while (len + need + 1 > cap)
        {
            cap *= 2;
            out = (char *)xrealloc(out, (size_t)cap);
        }
        if (i)
        {
            strcpy(out + len, sep);
            len += (int)strlen(sep);
        }
        strcpy(out + len, s);
        len += (int)strlen(s);
        free(s);
    }
    return string_take(vm, out);
}

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

static int read_i16_le(const unsigned char *b, int i)
{
    int v = (int)b[i] | ((int)b[i + 1] << 8);
    return v >= 32768 ? v - 65536 : v;
}

static MzValue n_godot_tilemap_data(MzVM *vm, int argc, MzValue *argv)
{
    if (argc < 1 || argv[0].type != MZ_STRING)
        return array_new(vm);
    const char *s = as_string(argv[0]);
    int slen = (int)strlen(s);
    int cap = (slen / 4) * 3 + 4;
    unsigned char *bytes = (unsigned char *)malloc((size_t)cap);
    if (!bytes)
        exit(1);
    int out = 0;
    int vals[4];
    int count = 0;
    for (int i = 0; i < slen; i++)
    {
        if (s[i] == '=')
            break;
        int v = b64_value(s[i]);
        if (v < 0)
            continue;
        vals[count++] = v;
        if (count == 4)
        {
            bytes[out++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
            bytes[out++] = (unsigned char)(((vals[1] & 15) << 4) | (vals[2] >> 2));
            bytes[out++] = (unsigned char)(((vals[2] & 3) << 6) | vals[3]);
            count = 0;
        }
    }
    if (count == 3)
    {
        bytes[out++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
        bytes[out++] = (unsigned char)(((vals[1] & 15) << 4) | (vals[2] >> 2));
    }
    else if (count == 2)
    {
        bytes[out++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
    }

    MzValue result = array_new(vm);
    for (int i = 0; i + 11 < out; i += 12)
    {
        MzValue cell = array_new(vm);
        array_push(as_array(cell), mz_number(read_i16_le(bytes, i + 2)));
        array_push(as_array(cell), mz_number(read_i16_le(bytes, i + 4)));
        array_push(as_array(cell), mz_number(read_i16_le(bytes, i + 6)));
        array_push(as_array(cell), mz_number(read_i16_le(bytes, i + 8)));
        array_push(as_array(cell), mz_number(read_i16_le(bytes, i + 10)));
        array_push(as_array(result), cell);
    }
    free(bytes);
    return result;
}

static MzValue n_godot_tilemap_file(MzVM *vm, int argc, MzValue *argv)
{
    if (argc < 2 || argv[0].type != MZ_STRING || argv[1].type != MZ_STRING)
        return array_new(vm);
    char *path = mz_to_cstring(argv[0]);
    char *node = mz_to_cstring(argv[1]);
    char *src = read_file(path);
    free(path);
    if (!src)
    {
        free(node);
        return array_new(vm);
    }

    char needle[256];
    snprintf(needle, sizeof(needle), "[node name=\"%s\"", node);
    free(node);
    char *section = strstr(src, needle);
    if (!section)
    {
        free(src);
        return array_new(vm);
    }
    char *next_section = strstr(section + 1, "\n[");
    char *data = strstr(section, "tile_map_data = PackedByteArray(\"");
    if (!data || (next_section && data > next_section))
    {
        free(src);
        return array_new(vm);
    }
    data += (int)strlen("tile_map_data = PackedByteArray(\"");
    char *end = strchr(data, '"');
    if (!end)
    {
        free(src);
        return array_new(vm);
    }
    MzValue encoded = string_take(vm, copy_span(data, (int)(end - data)));
    MzValue out = n_godot_tilemap_data(vm, 1, &encoded);
    free(src);
    return out;
}

static MzValue n_math1(MzVM *vm, int argc, MzValue *argv, double (*fn)(double))
{
    (void)vm;
    return mz_number(argc ? fn(mz_as_number(argv[0])) : 0);
}
static MzValue n_abs(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, fabs); }
static MzValue n_floor(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, floor); }
static MzValue n_ceil(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, ceil); }
static MzValue n_sqrt(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, sqrt); }
static MzValue n_sin(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, sin); }
static MzValue n_cos(MzVM *vm, int argc, MzValue *argv) { return n_math1(vm, argc, argv, cos); }
static MzValue n_pow(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    return mz_number(argc >= 2 ? pow(mz_as_number(argv[0]), mz_as_number(argv[1])) : 0);
}
static MzValue n_atan2(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    return mz_number(argc >= 2 ? atan2(mz_as_number(argv[0]), mz_as_number(argv[1])) : 0);
}
static MzValue n_min(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_null();
    double m = mz_as_number(argv[0]);
    for (int i = 1; i < argc; i++)
        if (mz_as_number(argv[i]) < m)
            m = mz_as_number(argv[i]);
    return mz_number(m);
}
static MzValue n_max(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_null();
    double m = mz_as_number(argv[0]);
    for (int i = 1; i < argc; i++)
        if (mz_as_number(argv[i]) > m)
            m = mz_as_number(argv[i]);
    return mz_number(m);
}
static MzValue n_clamp(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 3)
        return mz_null();
    double x = mz_as_number(argv[0]), lo = mz_as_number(argv[1]), hi = mz_as_number(argv[2]);
    if (x < lo)
        x = lo;
    if (x > hi)
        x = hi;
    return mz_number(x);
}
static MzValue n_rand(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    double max = argc ? mz_as_number(argv[0]) : 1;
    return mz_number(((double)rand() / (double)RAND_MAX) * max);
}
static MzValue n_rand_int(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    int max = argc ? (int)mz_as_number(argv[0]) : 100;
    if (max <= 0)
        max = 1;
    return mz_number(rand() % max);
}

static void stdlib(MzVM *vm)
{
    mz_define_native(vm, "print", n_print);
    mz_define_native(vm, "len", n_len);
    mz_define_native(vm, "str", n_str);
    mz_define_native(vm, "num", n_num);
    mz_define_native(vm, "type", n_type);
    mz_define_native(vm, "push", n_push);
    mz_define_native(vm, "pop", n_pop);
    mz_define_native(vm, "remove", n_remove);
    mz_define_native(vm, "has", n_has);
    mz_define_native(vm, "del", n_del);
    mz_define_native(vm, "range", n_range);
    mz_define_native(vm, "join", n_join);
    mz_define_native(vm, "godot_tilemap_data", n_godot_tilemap_data);
    mz_define_native(vm, "godot_tilemap_file", n_godot_tilemap_file);
    mz_define_native(vm, "abs", n_abs);
    mz_define_native(vm, "floor", n_floor);
    mz_define_native(vm, "ceil", n_ceil);
    mz_define_native(vm, "sqrt", n_sqrt);
    mz_define_native(vm, "sin", n_sin);
    mz_define_native(vm, "cos", n_cos);
    mz_define_native(vm, "pow", n_pow);
    mz_define_native(vm, "atan2", n_atan2);
    mz_define_native(vm, "min", n_min);
    mz_define_native(vm, "max", n_max);
    mz_define_native(vm, "clamp", n_clamp);
    mz_define_native(vm, "rand", n_rand);
    mz_define_native(vm, "rand_int", n_rand_int);
    env_define_span(vm->globals, "PI", 2, mz_number(3.14159265358979323846));
}

MzVM *mz_new(void)
{
    MzVM *vm = (MzVM *)calloc(1, sizeof(MzVM));
    if (!vm)
        exit(1);
    vm->globals = env_new(vm, NULL);
    vm->env = vm->globals;
    vm->return_value = mz_null();
    srand((unsigned)time(NULL));
    stdlib(vm);
    return vm;
}

void mz_free(MzVM *vm)
{
    if (!vm)
        return;
    vm->globals = NULL;
    gc(vm, NULL);
    free(vm->tokens);
    free(vm);
}

int mz_run_source(MzVM *vm, const char *src)
{
    free(vm->tokens);
    vm->tokens = NULL;
    vm->token_count = 0;
    vm->token_cap = 0;
    vm->failed = 0;
    vm->pos = 0;
    vm->end = 0;
    if (!lex(vm, src))
        return 0;
    Parser p;
    p.vm = vm;
    p.pos = 0;
    p.end = vm->token_count - 1;
    p.env = vm->globals;
    while (!at_end(&p) && !vm->failed)
        statement(&p);
    gc(vm, vm->globals);
    return !vm->failed;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = (char *)malloc((size_t)n + 1);
    if (!s)
        exit(1);
    size_t got = fread(s, 1, (size_t)n, f);
    fclose(f);
    s[got] = 0;
    return s;
}

int mz_run_file(MzVM *vm, const char *path)
{
    char *src = read_file(path);
    if (!src)
    {
        fail(vm, "nao foi possivel abrir '%s'", path);
        return 0;
    }
    int ok = mz_run_source(vm, src);
    free(src);
    return ok;
}
