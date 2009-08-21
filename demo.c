#include <stdio.h>
#include "dlfcn.h"

extern int debug_verbosity;

int kkget()
{
	return 2;
}

void demo(char *libname, char *symname)
{
    const char *dlerr;
    void *handle, *symbol;
    typedef int (*int_fn_void_t)(void);

    debug_verbosity = 8;

    printf("opening library [%s]\n", libname);
    dlerr = dlerror();
    handle = dlopen(libname, RTLD_NOW);
    dlerr = dlerror();
    if (dlerr != NULL) fprintf(stderr, "dlopen() error: %s\n", dlerr);

    printf("opening symbol [%s]\n", symname);
    symbol = dlsym(handle, symname);
    printf("*******%d*********\n", ((int_fn_void_t)symbol)());
    dlerr = dlerror();
    if (dlerr != NULL) fprintf(stderr, "dlsym() error: %s\n", dlerr);

    printf("opening symbol [%s] via RTLD_DEFAULT\n", "kkget");
    symbol = dlsym(RTLD_DEFAULT, "kkget");
    printf("*******%d*********\n", ((int_fn_void_t)symbol)());
    printf("closing library [%s]\n", libname);
    dlclose(handle);
    dlerr = dlerror();
    if (dlerr != NULL) fprintf(stderr, "dlclose() error: %s\n", dlerr);
    else printf("successfully opened symbol\n");
}
