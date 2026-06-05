#ifndef MZ_H
#define MZ_H

typedef struct MzVM MzVM;
typedef struct MzValue MzValue;

typedef MzValue (*MzNative)(MzVM *vm, int argc, MzValue *argv);

typedef enum
{
    MZ_NULL,
    MZ_NUMBER,
    MZ_BOOL,
    MZ_STRING,
    MZ_ARRAY,
    MZ_OBJECT,
    MZ_FUNCTION,
    MZ_NATIVE
} MzType;

struct MzValue
{
    MzType type;
    union
    {
        double number;
        int boolean;
        void *ptr;
        MzNative native;
    } as;
};

MzVM *mz_new(void);
void mz_free(MzVM *vm);
int mz_run_file(MzVM *vm, const char *path);
int mz_run_source(MzVM *vm, const char *src);
const char *mz_error(MzVM *vm);

void mz_define_native(MzVM *vm, const char *name, MzNative fn);

MzValue mz_null(void);
MzValue mz_number(double n);
MzValue mz_bool(int b);
MzValue mz_string(MzVM *vm, const char *s);
char *mz_to_cstring(MzValue v);
double mz_as_number(MzValue v);
int mz_truthy(MzValue v);
int mz_array_len(MzValue v);
MzValue mz_array_get(MzValue v, int index);

#ifdef MZ_WITH_SDL
void mz_sdl_register(MzVM *vm);
void mz_sdl_shutdown(void);
#endif

#endif
