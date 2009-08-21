/* $Id: pmelf.h,v 1.17 2009/03/12 04:42:14 strauman Exp $ */

/* 
 * Authorship
 * ----------
 * This software ('pmelf' ELF file reader) was created by
 *     Till Straumann <strauman@slac.stanford.edu>, 2008,
 * 	   Stanford Linear Accelerator Center, Stanford University.
 * 
 * Acknowledgement of sponsorship
 * ------------------------------
 * This software was produced by
 *     the Stanford Linear Accelerator Center, Stanford University,
 * 	   under Contract DE-AC03-76SFO0515 with the Department of Energy.
 * 
 * Government disclaimer of liability
 * ----------------------------------
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied, or
 * assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately owned
 * rights.
 * 
 * Stanford disclaimer of liability
 * --------------------------------
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * Stanford disclaimer of copyright
 * --------------------------------
 * Stanford University, owner of the copyright, hereby disclaims its
 * copyright and all other rights in this software.  Hence, anyone may
 * freely use it for any purpose without restriction.  
 * 
 * Maintenance of notices
 * ----------------------
 * In the interest of clarity regarding the origin and status of this
 * SLAC software, this and all the preceding Stanford University notices
 * are to remain affixed to any copy or derivative of this software made
 * or distributed by the recipient and are to be affixed to any copy of
 * software made or distributed by the recipient that contains a copy or
 * derivative of this software.
 * 
 * ------------------ SLAC Software Notices, Set 4 OTT.002a, 2004 FEB 03
 */ 
#ifndef _PMELF_H
#define _PMELF_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Elementary Types */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef  int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef  int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef  int64_t Elf64_Sxword;

/**************************************************/
/*  ELF HEADER                                    */
/**************************************************/

/* Object file type */
#define ET_NONE		0		/* none               */
#define ET_REL		1		/* relocatable        */
#define ET_EXEC		2		/* executable         */
#define ET_DYN		3		/* shared object      */
#define ET_CORE		4		/* core               */
#define ET_LOPROC	0xff00	/* processor specific */
#define ET_HIPROC	0xffff	/* processor specific */

/* Machine type (supported by us so far)          */
#define EM_386				 3
#define EM_68K				 4
#define EM_PPC				20
#define EM_X86_64			62

/* Identification indices                          */
#define EI_MAG0				 0
#define EI_MAG1				 1
#define EI_MAG2				 2
#define EI_MAG3				 3
#define EI_CLASS			 4
#define EI_DATA				 5
#define EI_VERSION			 6
#define EI_OSABI			 7
#define EI_PAD				 8

#define ELFMAG0			  0x7f
#define ELFMAG1			   'E'
#define ELFMAG2			   'L'
#define ELFMAG3			   'F'

#define ELFCLASSNONE		 0
#define ELFCLASS32			 1
#define ELFCLASS64			 2

#define ELFDATANONE			 0
#define ELFDATA2LSB			 1
#define ELFDATA2MSB			 2

#define ELFOSABI_NONE		 0
#define ELFOSABI_SYSV		 0 /* yep */

/* Object file version                             */
#define EV_NONE				 0
#define EV_CURRENT			 1

#define EI_NIDENT  16
typedef struct {
	uint8_t		e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	uint8_t		e_ident[EI_NIDENT];
	Elf64_Half	e_type;
	Elf64_Half	e_machine;
	Elf64_Word	e_version;
	Elf64_Addr	e_entry;
	Elf64_Off	e_phoff;
	Elf64_Off	e_shoff;
	Elf64_Word	e_flags;
	Elf64_Half	e_ehsize;
	Elf64_Half	e_phentsize;
	Elf64_Half	e_phnum;
	Elf64_Half	e_shentsize;
	Elf64_Half	e_shnum;
	Elf64_Half	e_shstrndx;
} Elf64_Ehdr;

typedef union {
	uint8_t    e_ident[EI_NIDENT];
	Elf32_Ehdr e32;
	Elf64_Ehdr e64;
} Elf_Ehdr;

/**************************************************/
/*  SECTION HEADER                                */
/**************************************************/

/* Section indices                                */
#define SHN_UNDEF			 0
#define SHN_LORESERVE		 0xff00
#define SHN_LOPROC			 0xff00
#define SHN_HIPROC			 0xff1f
#define SHN_ABS				 0xfff1
#define SHN_COMMON			 0xfff2
#define SHN_HIRESERVE		 0xffff

#define SHT_NULL			 0
#define SHT_PROGBITS		 1
#define SHT_SYMTAB			 2
#define SHT_STRTAB			 3
#define SHT_RELA			 4
#define SHT_HASH			 5
#define SHT_DYNAMIC			 6
#define SHT_NOTE			 7
#define SHT_NOBITS			 8
#define SHT_REL				 9
#define SHT_SHLIB			10
#define SHT_DYNSYM			11
#define SHT_INIT_ARRAY		14
#define SHT_FINI_ARRAY		15
#define SHT_PREINIT_ARRAY	16
#define SHT_GROUP			17
#define SHT_SYMTAB_SHNDX	18
#define SHT_MAXSUP			18
#define SHT_GNU_ATTRIBUTES  0x6ffffff5
#define SHT_GNU_VERSION     0x6fffffff
#define SHT_GNU_VERSION_R   0x6ffffffe
#define SHT_LOPROC			0x70000000
#define SHT_HIPROC			0x7fffffff
#define SHT_LOUSER			0x80000000
#define SHT_HIUSER			0xffffffff

#define SHF_WRITE			0x00000001
#define SHF_ALLOC			0x00000002
#define SHF_EXECINSTR		0x00000004
#define SHF_MERGE			0x00000010
#define SHF_STRINGS			0x00000020
#define SHF_INFO_LINK		0x00000040
#define SHF_LINK_ORDER		0x00000080
#define SHF_OS_NONCONFORMING	0x00000100
#define SHF_GROUP			0x00000200
#define SHF_MSKSUP			 (~0x3f7)
#define SHF_MASKPROC		0xf0000000

#define GRP_COMDAT			 1

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

typedef union {
	Elf32_Shdr s32;
	Elf64_Shdr s64;
} Elf_Shdr;

/**************************************************/
/*  SYMBOLS                                       */
/**************************************************/

#define ELF32_ST_BIND(x)		(((x)>>4)&0xf)
#define ELF32_ST_TYPE(x)		((x) & 0xf)
#define ELF32_ST_INFO(b,t)		(((b)<<4) | ((t)&0xf))

#define ELF64_ST_BIND(x)		ELF32_ST_BIND(x)
#define ELF64_ST_TYPE(x)		ELF32_ST_TYPE(x)
#define ELF64_ST_INFO(b,t)		ELF32_ST_INFO(b,t)

#define STB_LOCAL		 0
#define STB_GLOBAL		 1
#define STB_WEAK		 2
#define STB_MAXSUP		 2
#define STB_LOPROC		13
#define STB_HIPROC		15

#define STT_NOTYPE		 0
#define STT_OBJECT		 1
#define STT_FUNC		 2
#define STT_SECTION		 3
#define STT_FILE		 4
#define STT_COMMON		 5
#define STT_TLS			 6
#define STT_MAXSUP		 6
#define STT_LOPROC		13
#define STT_HIPROC		15

#define STV_DEFAULT		 0
#define STV_INTERNAL	 1
#define STV_HIDDEN		 2
#define STV_PROTECTED	 3

#define ELF32_ST_VISIBILITY(o) ((o)&3)
#define ELF64_ST_VISIBILITY(o) ELF32_ST_VISIBILITY(o)

typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	uint8_t		st_info;
	uint8_t		st_other;
	Elf32_Half	st_shndx;
} Elf32_Sym;

typedef struct {
	Elf64_Word	st_name;
	uint8_t		st_info;
	uint8_t		st_other;
	Elf64_Half	st_shndx;
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;
} Elf64_Sym;

typedef union {
	Elf32_Sym t32;
	Elf64_Sym t64;
} Elf_Sym;

/**************************************************/
/*  RELOCATION RECORDS                            */
/**************************************************/

/*
 * SysvR4 relocation types for i386. Statically linked objects
 * just use R_386_32 and R_386_PC32 which makes our job really easy...
 */
#define R_386_NONE                0
#define R_386_32                  1
#define R_386_PC32                2
#define R_386_GOT32               3
#define R_386_PLT32               4
#define R_386_COPY                5
#define R_386_GLOB_DAT            6
#define R_386_JMP_SLOT            7
#define R_386_RELATIVE            8
#define R_386_GOTOFF              9
#define R_386_GOTPC              10

/*
 * 68k relocation types
 */
#define R_68K_NONE                0
#define R_68K_32                  1
#define R_68K_16                  2
#define R_68K_8                   3
#define R_68K_PC32                4
#define R_68K_PC16                5
#define R_68K_PC8                 6
#define R_68K_GOT32               7
#define R_68K_GOT16               8
#define R_68K_GOT8                9
#define R_68K_GOT320             10
#define R_68K_GOT160             11
#define R_68K_GOT80              12
#define R_68K_PLT32              13
#define R_68K_PLT16              14
#define R_68K_PLT8               15
#define R_68K_PLT320             16
#define R_68K_PLT160             17
#define R_68K_PLT80              18
#define R_68K_COPY               19
#define R_68K_GLOB_DAT           20
#define R_68K_JMP_SLOT           21
#define R_68K_RELATIVE           22


/* PPC relocation types */
#define R_PPC_NONE                0
#define R_PPC_ADDR32              1
#define R_PPC_ADDR24              2
#define R_PPC_ADDR16              3
#define R_PPC_ADDR16_LO           4
#define R_PPC_ADDR16_HI           5
#define R_PPC_ADDR16_HA           6
#define R_PPC_ADDR14              7
#define R_PPC_ADDR14_BRTAKEN      8
#define R_PPC_ADDR14_BRNTAKEN     9
#define R_PPC_REL24              10
#define R_PPC_REL14              11
#define R_PPC_REL14_BRTAKEN      12
#define R_PPC_REL14_BRNTAKEN     13
#define R_PPC_GOT16              14
#define R_PPC_GOT16_LO           15
#define R_PPC_GOT16_HI           16
#define R_PPC_GOT16_HA           17
#define R_PPC_PLTREL24           18
#define R_PPC_COPY               19
#define R_PPC_GLOB_DAT           20
#define R_PPC_JMP_SLOT           21
#define R_PPC_RELATIVE           22
#define R_PPC_LOCAL24PC          23
#define R_PPC_UADDR32            24
#define R_PPC_UADDR16            25
#define R_PPC_REL32              26
#define R_PPC_PLT32              27
#define R_PPC_PLTREL32           28
#define R_PPC_PLT16_LO           29
#define R_PPC_PLT16_HI           30
#define R_PPC_PLT16_HA           31
#define R_PPC_SDAREL16           32
#define R_PPC_SECTOFF            33
#define R_PPC_SECTOFF_LO         34
#define R_PPC_SECTOFF_HI         35
#define R_PPC_SECTOFF_HA         36
#define R_PPC_ADDR30             37

/*
 * Sparc ELF relocation types
 */
#define	R_SPARC_NONE		0
#define	R_SPARC_8		1
#define	R_SPARC_16		2
#define	R_SPARC_32		3
#define	R_SPARC_DISP8		4
#define	R_SPARC_DISP16		5
#define	R_SPARC_DISP32		6
#define	R_SPARC_WDISP30		7
#define	R_SPARC_WDISP22		8
#define	R_SPARC_HI22		9
#define	R_SPARC_22		10
#define	R_SPARC_13		11
#define	R_SPARC_LO10		12
#define	R_SPARC_GOT10		13
#define	R_SPARC_GOT13		14
#define	R_SPARC_GOT22		15
#define	R_SPARC_PC10		16
#define	R_SPARC_PC22		17
#define	R_SPARC_WPLT30		18
#define	R_SPARC_COPY		19
#define	R_SPARC_GLOB_DAT	20
#define	R_SPARC_JMP_SLOT	21
#define	R_SPARC_RELATIVE	22
#define	R_SPARC_UA32		23
#define R_SPARC_PLT32		24
#define R_SPARC_HIPLT22		25
#define R_SPARC_LOPLT10		26
#define R_SPARC_PCPLT32		27
#define R_SPARC_PCPLT22		28
#define R_SPARC_PCPLT10		29
#define R_SPARC_10		30
#define R_SPARC_11		31
#define R_SPARC_64		32
#define R_SPARC_OLO10		33
#define R_SPARC_WDISP16		40
#define R_SPARC_WDISP19		41
#define R_SPARC_7		43
#define R_SPARC_5		44
#define R_SPARC_6		45


/*
 * SVR4 relocation types for x86_64
 */
#define R_X86_64_NONE             0
#define R_X86_64_64               1
#define R_X86_64_PC32             2
#define R_X86_64_GOT32            3
#define R_X86_64_PLT32            4
#define R_X86_64_COPY             5
#define R_X86_64_GLOB_DAT         6
#define R_X86_64_JUMP_SLOT        7
#define R_X86_64_RELATIVE         8
#define R_X86_64_GOTPCREL         9
#define R_X86_64_32              10
#define R_X86_64_32S             11
#define R_X86_64_16              12
#define R_X86_64_PC16            13
#define R_X86_64_8               14
#define R_X86_64_PC8             15
#define R_X86_64_DTPMOD64        16
#define R_X86_64_DTPOFF64        17
#define R_X86_64_TPOFF64         18
#define R_X86_64_TLSGD           19
#define R_X86_64_TLSLD           20
#define R_X86_64_DTPOFF32        21
#define R_X86_64_GOTTPOFF        22
#define R_X86_64_TPOFF32         23
#define R_X86_64_PC64            24
#define R_X86_64_GOTOFF64        25
#define R_X86_64_GOTPC32         26
#define R_X86_64_SIZE32          32
#define R_X86_64_SIZE64          33
#define R_X86_64_GOTPC32_TLSDESC 34
#define R_X86_64_TLSDESC_CALL    35
#define R_X86_64_TLSDESC         36

#define ELF32_R_SYM(x)		((x) >> 8)
#define ELF32_R_TYPE(x) 	((uint8_t)((x)&0xff))
#define ELF32_R_INFO(s,t)	(((s)<<8) | ((t)&0xff))

#define ELF64_R_SYM(x)		((x) >> 32)
#define ELF64_R_TYPE(x) 	((uint32_t)((x)&0xffffffffL))
#define ELF64_R_INFO(s,t)	(((s)<<32) | ((t)&0xffffffffL))

typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
	Elf32_Sword	r_addend;
} Elf32_Rela;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
} Elf64_Rel;

typedef struct {
	Elf64_Addr		r_offset;
	Elf64_Xword		r_info;
	Elf64_Sxword	r_addend;
} Elf64_Rela;

typedef union {
	Elf32_Rel	r32;
	Elf32_Rela  ra32;
	Elf64_Rel	r64;
	Elf64_Rela  ra64;
} Elf_Reloc;


/**************************************************/
/* ANYTHING BELOW HERE IS DEFINED BY THIS LIBRARY */
/* AND NOT BY THE ELF FILE FORMAT.                */
/**************************************************/

typedef union {
	Elf32_Ehdr *p_e32;
	Elf64_Ehdr *p_e64;
} Elf_PEhdr;

typedef union {
	uint8_t    *p_raw;
	Elf32_Shdr *p_s32;
	Elf64_Shdr *p_s64;
} Elf_PShdr;

typedef union {
	uint8_t    *p_raw;
	Elf32_Sym  *p_t32;
	Elf64_Sym  *p_t64;
} Elf_PSym;

#define ELF_ST_BIND(x)		((x) >> 4)
#define ELF_ST_TYPE(x)		(((unsigned int) x) & 0xf)

#ifdef __cplusplus
}
#endif


#endif
