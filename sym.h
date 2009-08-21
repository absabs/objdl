#ifndef _LINKER_SYM_H_
#define _LINKER_SYM_H_
struct dl_symbol
{
        const char *name;
        unsigned long value;
};
struct dl_symbol_list
{
	struct dl_symbol sym;
	struct dl_symbol_list *next;
};

#endif
