#include "../include/mz.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("uso: mz arquivo.man");
        return 1;
    }
    MzVM *vm = mz_new();
    int ok = mz_run_file(vm, argv[1]);
    if (!ok)
        fprintf(stderr, "Erro: %s\n", mz_error(vm));
    mz_free(vm);
    return ok ? 0 : 1;
}
