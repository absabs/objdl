/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "dlfcn.h"
#include <pthread.h>
#include "linker.h"

/* This file hijacks the symbols stubbed out in libdl.so. */

#define DL_SUCCESS                    0
#define DL_ERR_CANNOT_FIND_LIBRARY    1
#define DL_ERR_INVALID_LIBRARY_HANDLE 2
#define DL_ERR_BAD_SYMBOL_NAME        3
#define DL_ERR_SYMBOL_NOT_FOUND       4
#define DL_ERR_SYMBOL_NOT_GLOBAL      5

static const char *dl_errors[] = {
    [DL_SUCCESS] = NULL,
    [DL_ERR_CANNOT_FIND_LIBRARY] = "Cannot find library",
    [DL_ERR_INVALID_LIBRARY_HANDLE] = "Invalid library handle",
    [DL_ERR_BAD_SYMBOL_NAME] = "Invalid symbol name",
    [DL_ERR_SYMBOL_NOT_FOUND] = "Symbol not found",
    [DL_ERR_SYMBOL_NOT_GLOBAL] = "Symbol is not global",
};

static int dl_last_err = DL_SUCCESS;

#define likely(expr)   __builtin_expect (expr, 1)
#define unlikely(expr) __builtin_expect (expr, 0)

static pthread_mutex_t dl_lock = PTHREAD_MUTEX_INITIALIZER;

void *dlopen(const char *filename, int flag) 
{
	soinfo *ret;

	pthread_mutex_lock(&dl_lock);
	ret = find_library(filename);
	if (unlikely(ret == NULL)) {
		dl_last_err = DL_ERR_CANNOT_FIND_LIBRARY;
	} else {
		ret->refcount++;
	}
	pthread_mutex_unlock(&dl_lock);
	return ret;
}

const char *dlerror(void)
{
    const char *err = dl_errors[dl_last_err];
    dl_last_err = DL_SUCCESS;
    return err;
}
/*
void *dlsym(void *handle, const char *symbol)
{
    unsigned base;
    Elf32_Sym *sym;
    unsigned bind;

    pthread_mutex_lock(&dl_lock);
    
    if(unlikely(handle == 0)) { 
        dl_last_err = DL_ERR_INVALID_LIBRARY_HANDLE;
        goto err;
    }
    if(unlikely(symbol == 0)) {
        dl_last_err = DL_ERR_BAD_SYMBOL_NAME;
        goto err;
    }
    
    if(handle == RTLD_DEFAULT) {
        sym = lookup(symbol, &base);
    } else if(handle == RTLD_NEXT) {
        sym = lookup(symbol, &base);
    } else {
        sym = lookup_in_library((soinfo*) handle, symbol);
        base = ((soinfo*) handle)->base;
    }

    if(likely(sym != 0)) {
        bind = ELF32_ST_BIND(sym->st_info);
    
        if(likely((bind == STB_GLOBAL) && (sym->st_shndx != 0))) {
            unsigned ret = sym->st_value + base;
            pthread_mutex_unlock(&dl_lock);
            return (void*)ret;
        }

        dl_last_err = DL_ERR_SYMBOL_NOT_GLOBAL;
    }
    else dl_last_err = DL_ERR_SYMBOL_NOT_FOUND;

err:
    pthread_mutex_unlock(&dl_lock);
    return 0;
}*/

int dlclose(void *handle)
{
	pthread_mutex_lock(&dl_lock);
	(void)unload_library((soinfo*)handle);
	pthread_mutex_unlock(&dl_lock);
	return 0;
}
