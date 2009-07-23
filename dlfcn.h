/*
 * for dynamic object file loading
 * The Open Group Base Specifications Issue 6 IEEE Std 1003.1, 2004 Edition 
 */
#ifndef __DLFCN_H__
#define __DLFCN_H__

extern void *dlopen(const char*  filename, int flag);
extern int dlclose(void*  handle);
extern const char *dlerror(void);
extern void *dlsym(void*  handle, const char*  symbol);

enum {
  RTLD_NOW  = 0,
  RTLD_LAZY = 1,

  RTLD_LOCAL  = 0,
  RTLD_GLOBAL = 2,
};

#define RTLD_NEXT       ((void *) -1)
#define RTLD_DEFAULT    ((void *) -2)

#endif /* __DLFCN_H */
