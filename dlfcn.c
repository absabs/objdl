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
#include "cexplock.h"
#include "dlfcn.h"
#include "linker.h"

#define SYSSYMFILE	"sym.map.gz"
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

static CexpLock dl_lock;

void *dlopen(const char *filename, int flag) 
{
	soinfo *ret;
	static int initialized = 0;

	if (!initialized) {
		cexpLockCreate(&dl_lock);
		__linker_init(SYSSYMFILE);
		initialized = 1;
	}
	
	cexpLock(dl_lock);

	ret = find_library(filename);
	if (unlikely(ret == NULL)) {
		dl_last_err = DL_ERR_CANNOT_FIND_LIBRARY;
	} else {
		ret->refcount++;
	}
	cexpUnlock(dl_lock);
	return ret;
}

const char *dlerror(void)
{
    const char *err = dl_errors[dl_last_err];
    dl_last_err = DL_SUCCESS;
    return err;
}

void *dlsym(void *handle, const char *symbol)
{
    unsigned long sym;

    cexpLock(dl_lock);
    
    if(unlikely(handle == 0)) { 
        dl_last_err = DL_ERR_INVALID_LIBRARY_HANDLE;
        goto err;
    }
    if(unlikely(symbol == 0)) {
        dl_last_err = DL_ERR_BAD_SYMBOL_NAME;
        goto err;
    }
    
    if(handle == RTLD_DEFAULT) {
        sym = lookup(symbol);
    } else if(handle == RTLD_NEXT) {
        sym = lookup(symbol);
    } else {
        sym = lookup_in_library((soinfo*) handle, symbol);
    }
    
    if(likely(sym != 0)) {
        cexpUnlock(dl_lock);
        return (void*)sym;
    }
    else dl_last_err = DL_ERR_SYMBOL_NOT_FOUND;

err:
    cexpUnlock(dl_lock);
    return 0;
}

int dlclose(void *handle)
{
	cexpLock(dl_lock);
	(void)unload_library((soinfo*)handle);
	cexpUnlock(dl_lock);
	return 0;
}
