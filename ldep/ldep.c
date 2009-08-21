/* $Id: ldep.c,v 1.31 2006-08-11 23:55:59 guest Exp $ */

/* tool for library/object file dependency analysis */

/* Author: Till Straumann <strauman@slac.stanford.edu>, 2003 */

/* scan symbol tables generated using 'nm -f posix' and
 * obeying the format:
 *
 * <library_name>'['<archive_member_name>']:'
 * <symbol_name>' '<class_char>' '[<start>' '<end>]
 *
 * The tool builds a database of all object files and another
 * one containing all symbols.
 *
 * Each object file holds lists of pointers to all symbols
 * it imports and exports, respectively.
 *
 * Each symbol object holds a pointer to the object where
 * it is defined and a list of objects importing the symbol.
 *
 * Using these datastructures, the tool can 'link' objects
 * together and construct dependency information.
 */

/*
 * Copyright 2003, Stanford University and
 * 		Till Straumann <strauman@@slac.stanford.edu>
 * 
 * Stanford Notice
 * ***************
 * 
 * Acknowledgement of sponsorship
 * * * * * * * * * * * * * * * * *
 * This software was produced by the Stanford Linear Accelerator Center,
 * Stanford University, under Contract DE-AC03-76SFO0515 with the Department
 * of Energy.
 * 
 * Government disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied,
 * or assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately
 * owned rights.
 * 
 * Stanford disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * This product is subject to the EPICS open license
 * - - - - - - - - - - - - - - - - - - - - - - - - - 
 * Consult the LICENSE file or http://www.aps.anl.gov/epics/license/open.php
 * for more information.
 * 
 * Maintenance of notice
 * - - - - - - - - - - -
 * In the interest of clarity regarding the origin and status of this
 * software, Stanford University requests that any recipient of it maintain
 * this notice affixed to any distribution by the recipient that contains a
 * copy or derivative of this software.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <search.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/*
 * some debugging flags are actually 'verbosity' flags
 * and are also used if DEBUG is undefined
 */
#define DEBUG_SCAN		(1<<0)
#define DEBUG_TREE		(1<<1)
#define DEBUG_WALK		(1<<2)
#define DEBUG_LINK		(1<<3)
#define DEBUG_UNLINK	(1<<4)
#define DEBUG_COMMENT	(1<<5)

#undef	DEBUG

#define NWORK

#define LINKER_VERSION_SEPARATOR '@'
#define DUMMY_ALIAS_PREFIX       "__cexp_dummy_alias_"

/* TYPE DECLARATIONS */

/* 'forward' declaration of pointer types */

typedef struct ObjFRec_		*ObjF;
typedef struct SymRec_		*Sym;
typedef struct XrefRec_		*Xref;
typedef struct LinkSetRec_	*LinkSet;
typedef struct LibRec_		*Lib;

typedef struct LinkSetRec_ {
	char	*name;			/* name of the link set */
	ObjF	set;			/* head of a linked list of objects belonging to the link set; the
							 * members are linked by their 'link.next' field (LinkNodeRec)
							 */
} LinkSetRec;

typedef struct LinkNodeRec {
	LinkSet	anchor;			/* pointer to the link set this object belongs to (if any) */
	ObjF	next;			/* next member of the link set */
} LinkNodeRec;

/*
 * WARNING: DONT change the order of the fields, nor insert anything
 *          without adapting initialization of the 'undefSymPod'
 */
typedef struct ObjFRec_ {
	char		*name;		/* name of this object file */
	ObjF		next;		/* linked list of all objects */
	Lib			lib;		/* libary we're part of or NULL */
	LinkNodeRec link; 		/* link set we're a member of */
	ObjF		work;		/* temp pointer to do work */
#ifndef NWORK
	ObjF		work1;
#endif
	int			nexports;
	Xref		exports;	/* symbols exported by this object */
	int			nimports;
	Xref		imports;	/* symbols exported by this object */
} ObjFRec;

typedef struct LibRec_ {
	char	*name;
	Lib		next;		/* linked list of libraries */
	int		nfiles;
	ObjF	*files;		/* pointer to an array of library members */
} LibRec;


typedef void (*DepWalkAction)(ObjF f, int depth, void *closure);

typedef struct DepPrintArgRec_ {
	int		minDepth;
	int 	indent;
	int 	depthIndent;
	FILE	*file;
} DepPrintArgRec, *DepPrintArg;

/* Argument for processFile() */
typedef struct ProcTabRec_ {
	char	*fname;
	int		linkNotUnlink;
} ProcTabRec, *ProcTab;


/* access the symbol 'TYPE' embedded in its name string */
#define TYPE(sym) (*((sym)->name - 1))

#ifdef  TYPE
#define TYPESZ	1
#else
#define TYPESZ  0
#endif

#ifdef __GNUC__
#define INLINE __inline__
#else
#define INLINE
#endif

/* struct describing a symbol */
typedef struct SymRec_ {
	char	*name;			/* we point 'name' to a string and store the type in 'name[-1]' */
	int		size;			/* size of object associated with symbol */
	Xref	exportedBy;		/* linked list of cross-references to objects exporting this symbol */
	Xref	importedFrom;	/* anchor of a linked list of cross-references to objects importing this symbol */
} SymRec;

/* struct describing a symbol 'reference', i.e. a 'connection' between a symbol and an object file */
typedef struct XrefRec_ {
	Sym				sym;		/* symbol and */
	ObjF			obj;		/* object file we 'interconnect' */
	unsigned long	multiuse;	/* BITFIELD (assuming pointers are word aligned; LSB is 'weak' flag)
								 * (questionable attempt to save memory...)
								 */
} XrefRec;

/* macros to access 'BITFIELD' members */
#define XREF_FLAGS 		1	/* mask for the flag bit parts of BITFIELD */
#define XREF_FLG_WEAK	1	/* 'weak' symbol attribute flag */

#define XREF_NEXT(ref) ((Xref)((ref)->multiuse & ~XREF_FLAGS))	/* access of the 'next' pointer (BITFIELD member) */
#define XREF_WEAK(ref) ((ref)->multiuse & XREF_FLG_WEAK)		/* access of the 'weak' flag (BITFIELD member) */

/* inline routines to access 'BITFIELD' */

/* set the 'next' pointer (BITFIELD member) */
static INLINE void xref_set_next(Xref e, Xref next)
{
	e->multiuse &= XREF_FLAGS;
	e->multiuse |= ((unsigned long)next) & ~XREF_FLAGS;
}
   
/* set the 'weak' flag (BITFIELD member) */
static INLINE void xref_set_weak(Xref e, int weak)
{
	if (weak)
		e->multiuse |= XREF_FLG_WEAK;
	else
		e->multiuse &= ~XREF_FLG_WEAK;
}

/* GLOBAL FLAG VARIABLES */

static int	verbose =
#ifdef DEBUG
	DEBUG
#else
	0
#endif
	;

static int	force = 0;

#define WARN_UNDEFINED_SYMS (1<<0)

#ifndef DEFAULT_WARN_FLAGS
#ifdef DEBUG
#define DEFAULT_WARN_FLAGS (-1)
#else
#define DEFAULT_WARN_FLAGS 0
#endif
#endif

static int warn = DEFAULT_WARN_FLAGS;

/* FUNCTION FORWARD DECLARATIONS */

static void depwalk_rec(ObjF f, int depth);
static void depPrint(ObjF f, int depth, void *closure);

/* depwalk mode bits */
#define WALK_BUILD_LIST	(1<<0)
#define WALK_EXPORTS	(1<<1)
#define WALK_IMPORTS	(0)

void depwalk(ObjF f, DepWalkAction action, void *closure, int mode);
int  checkCircWorkList(ObjF f);
void depwalkListRelease(ObjF f);
void workListIterate(ObjF f, DepWalkAction action, void *closure);

/* VARIABLE FORWARD DECLARATIONS */
extern LinkSetRec appLinkSet;
extern LinkSetRec undefLinkSet;
extern LinkSetRec optionalLinkSet;

/* our fantastic string allocator */

#define STRCHUNK	10000

/* string space allocator (for strings living forever) */
char *stralloc(int len)
{
static char		*buf;
static int		avail=0;
char			*rval;

	assert(len<=STRCHUNK);

	if (len > avail) {
		avail = STRCHUNK;
		assert( buf=malloc(avail) );
	}

	rval   = buf;
	buf   += len;
	avail -= len;
	return rval;
}

#define MAXBUF	500

#define NMFMT(max)  "%"#max"s"
#define XNMFMT(m)	NMFMT(m)
#define STRFMT		XNMFMT(MAXBUF)"%*[ \t]"
/* #define FMT(max)	"%"#max"s%*[ \t]%c%*[^\n] \n" */

/* format string to scan 'nm -g -fposix' output */
#define THEFMT 		"%c%x%x"

/* GLOBAL VARIABLES */

/* streams where to print debugging and log messages, repectively */
static FILE *debugf, *logf;

/*
 * a "special" object exporting all symbols not defined
 * anywhere else
 */
static ObjFRec undefSymPod = {
	"<UNDEFINED>",		/* name */
	0,					/* next */
	0,					/* lib  */
	{ &undefLinkSet },	/* link */
};

/*
 * The three main link sets: "Application", "Optional" and "Undefined" (for objects
 * referencing undefined symbols)
 */
LinkSetRec appLinkSet   = {
	"Application",
	0,
};
LinkSetRec undefLinkSet = {
	"UNDEFINED",
	&undefSymPod,
};
LinkSetRec optionalLinkSet = {
	"Optional",
	0
};

/* list of all known files */
ObjF fileListHead=&undefSymPod, fileListTail=&undefSymPod;
static int	numFiles = 1; /* undefFilePod */

/* list of all known libraries */
Lib  libListHead=0, libListTail=0;
static int  numLibs  = 0;

/* sorted index (by name) of all known object files */
ObjF *fileListIndex = 0;

/* The global symbol table (a binary tree) */
void *symTbl = 0;

/* Global table of search paths */
static char *defPaths[]={"."};

char **searchPaths    = 0;
int    numSearchPaths = 0;
int    maxPathLen     = 0;

/*
 * Access the first real file (the very first node in the list
 * is the 'undefSymPod', i.e. a container holding/"exporting" all
 * undefined symbols
 */

static INLINE ObjF
fileListFirst()
{
	/* skip the undefined pod */
	return fileListHead ? fileListHead->next : 0;
}


/* Compare two symbols by their name (suitable for tsearch/tfind) */
static int
symcmp(const void *a, const void *b)
{
const Sym sa=(const Sym)a, sb=(const Sym)b;
	return strcmp(sa->name, sb->name);
}

/* Compare two library names stripping any path from either */
static int
libcmp(const char *a, const char *b)
{
register char *tmp;
	if ( tmp = strrchr(a,'/') )
		a = tmp+1;
	if ( tmp = strrchr(b,'/') )
		b = tmp+1;
	return strcmp(a,b);
}

/* find and open a file */
FILE *
ffind(char *fnam)
{
FILE *rval = 0;
char *buf  = malloc(strlen(fnam) + maxPathLen + 2);
int  i;

	for (i=0; i<numSearchPaths && !rval; i++) {
		sprintf(buf,"%s/%s",searchPaths[i],fnam);
		rval = fopen(buf,"r");
	}
	free(buf);
	return rval;
}
	
/*
 * Fixup an object, i.e. set final values for pointers at a time
 * when no more 'reallocs' need to be done.
 */
static void
fixupObj(ObjF f)
{
Sym		sym;
Xref	ex;
int		i;

	if ( !f )
		return;

	/* fixup export list; we can do this only after all reallocs have been performed */

	for (i=0, ex=f->exports; i<f->nexports; i++,ex++) {
		/* append to list of modules exporting this symbol */
		sym = ex->sym;
		if ( sym->exportedBy ) {
			Xref etmp;
			for ( etmp = sym->exportedBy; XREF_NEXT(etmp); etmp=XREF_NEXT(etmp) )
			  	/* nothing else to do */;
				xref_set_next(etmp, ex);
		} else {
			sym->exportedBy = ex;
		}
	}
}

/* Create a library object */
static Lib
createLib(char *name)
{
Lib	rval;

	assert( rval = calloc(1, sizeof(*rval)) );
	assert( rval->name = stralloc(strlen(name) + 1) );
	strcpy( rval->name, name );
	if (libListTail)
		libListTail->next = rval;
	else
		libListHead	= rval;
	libListTail = rval;
	numLibs++;
	return rval;
}

/* Add an object file to a library */
void
libAddObj(char *libname, ObjF obj)
{
Lib l;
int i;
	for (l=libListHead; l; l=l->next) {
		if ( !libcmp(l->name, libname) )
			break;
	}
	if ( !l ) {
		/* must create a new library */
		l = createLib(libname);
	}
	/* sanity check */
	for ( i=0; i<l->nfiles; i++) {
		assert( strcmp(l->files[i]->name, obj->name) );
	}
	i = l->nfiles+1;
	assert( l->files = realloc(l->files, i * sizeof(*l->files)) );
	l->files[l->nfiles] = obj;
	l->nfiles = i;
	obj->lib  = l;
}

/*
 * Split a string of the format
 *
 *   <filename> | <libname> '[' <filename> ']' into
 * 
 * <libname> and <filename>, *ppo and *ppc hold pointers
 * to the opening '[' and closing ']' or  NULL if only
 * the <filename> is present.
 *
 * RETURNS: pointer to <filename> or NULL if the above
 *          syntax is not obeyed.
 */
static char *
splitName(char *name, char **ppo, char **ppc)
{
char	*rval = name;
int		l     = strlen(name);

	*ppo = 0;

	/* is it part of a library ? */
	if ( l>0 && ']'== *(*ppc=name+(l-1)) ) {
		if ( !(*ppo = strrchr(name,'[')) ) {
			fprintf(stderr,"ERROR: misformed archive member name: %s\n",name);
			fprintf(stderr,"       'library[member]' expected\n");
			return 0;
		}
		**ppo  = 0;
		**ppc  = 0;
		rval =  (*ppo)+1;
	}
	return rval;
}

/*
 * Print the name of an object to 'feil', prepending
 * the library name if 'f' is member of a library
 */
static int
printObjName(FILE *feil, ObjF f)
{
Lib l = f->lib;
char *lname = "";

	if ( l ) {
		if ((lname = strrchr(l->name,'/'))) {
			lname++;
		} else  {
			lname = l->name;
		}
	}
	return fprintf(feil, l ? "%s[%s]" : "%s%s", lname, f->name);
}

/* Create an object file 'object' */
static ObjF
createObj(char *name)
{
ObjF obj;
int  l = strlen(name);
char *po,*pc,*objn;

	/* is it part of a library ? */
	objn = splitName(name, &po, &pc);

	if (!objn)
		exit(1); /* found an ill-formed name; fatal */

	assert( obj = calloc(1, sizeof(*obj)) );

	/* build/copy name */
	assert( obj->name = stralloc(strlen(objn) + 1) );
	strcpy( obj->name, objn );

	/* append to list of objects */
	fileListTail->next = obj;
	fileListTail = obj;
	numFiles++;

	if (po) {
		/* part of a library */
		libAddObj(name, obj);
		*po = '[';
		*pc = ']';
	}

	return obj;
}


#define TOUPPER(ch)	(force ? toupper(ch) : (ch)) /* less paranoia when checking symbol types */

/* Scan a file generated with 'nm -g -fposix' */
int
scan_file(FILE *f, char *name)
{
char	buf[MAXBUF+1];
char	*rest;
int		got;
char	type, otype;
int		line=0;
ObjF	obj =0;
int		len;
int		weak;
int		size;
int		val;
Sym		nsym = 0,sym;
Sym		*found;

	/* tag end of buffer */
	buf[MAXBUF]='X';

	while ( fgets(buf, sizeof(buf), f) ) {

		/* scan the initial string and chop everything beyond the first whitespace off */
		for (rest = buf; *rest && ' '!= *rest && '\t'!=*rest && '\n'!=*rest; rest++)
			/* nothing else to do */;

		got = (rest == buf) ? 0 : 1;
		if ( *rest )
			*rest++ = 0;
		
		if ( got && *rest )
			got += sscanf(rest, THEFMT, &otype, &val, &size);

		line++;

		if ( !buf[MAXBUF] ) {
			fprintf(stderr,"Scanner buffer overrun\n");
			return -1;
		}
		switch (got) {
			default:
				fprintf(stderr,"Unable to read %s/line %i (%i conversions of '%s')\n",name,line,got,STRFMT""THEFMT);
				return -1;

			case 1:
				len = strlen(buf);
				if ( ':' != buf[len-1] ) {
					fprintf(stderr,"<FILENAME> in %s/line %i not ':' terminated - did you use 'nm -fposix?'\n", name, line);
					return -1;
				}
				fixupObj(obj);

				/* strip trailing ':' */
				buf[--len]=0;

				obj = createObj(buf);

#if DEBUG & DEBUG_SCAN
				fprintf(debugf,"In FILE: '%s'\n", buf);
#endif
			break;

			case 2:
			case 3: size = -1;
			case 4:
				switch (got) {
					case 2:	size = -1; break; /* probably an undefined symbol */
					case 3: size =  0; break; /* some defined syms have size not set -- treat as 0 */
					default: break;
				}
				if (!obj) {
					char *dot, *slash,*nmbuf;
					fprintf(stderr,"Warning: Symbol without object file??\n");

					assert( nmbuf = malloc(strlen(name)+5) );

					strcpy( nmbuf, name );
					slash = strrchr(nmbuf, '/');
					dot   = strrchr(nmbuf, '.');
					if ( !dot || (slash && slash > dot) ) {
						strcat(nmbuf, ".o");
					} else {
						strcpy(dot+1,"o");
					}
					slash = slash ? slash + 1 : nmbuf;
					obj = createObj( slash );

				    fprintf(stderr,"-> substituting symbol file name, using '%s'... (%s/line %i)\n",slash,name,line);
					free(nmbuf);
				}

				type = TOUPPER(otype);

				if ( !nsym )
					assert( nsym = calloc(1,sizeof(*nsym)) );

				nsym->name = buf;

				if ( -1==size && 'U' != type ) {
					fprintf(stderr,"Warning: '%s' (type '%c') has unknown size; setting to zero\n",
							nsym->name, type);
					size = 0;
				}

				nsym->size = size;
				
				assert( found = (Sym*) tsearch(nsym, &symTbl, symcmp) );
				if ( *found == nsym ) {
#if DEBUG & DEBUG_TREE
					fprintf(debugf,"Adding new symbol %s (found %p, sym %p)\n",(*found)->name, found, *found);
#endif
					nsym->name = stralloc(strlen(buf) + 1 + TYPESZ) + TYPESZ; /* store the type in name[-1] */
					strcpy(nsym->name, buf);
#ifdef TYPE
					TYPE(nsym) = (char)otype;
#endif
					nsym = 0;
				} else {
#if DEBUG & DEBUG_TREE
					fprintf(debugf,"Found existing symbol %s (found %p, sym %p)\n",(*found)->name, found, *found);

#endif

#ifdef TYPE
					if (  type != TOUPPER(TYPE(*found)) ) {
						int warn, override, nweak;

						/* for some unknown reason, there seem to be global symbols
						 * of type 'w' in the compiler startfiles...
						 */
						nweak= 'W' == type || 'V' == type || 'w' == type;
#ifdef __GNUC__
#warning TODO weak symbols
#endif

						warn = ( 'U' != TYPE(*found) && 'U' != type );
						
				 		if (warn) {
							fprintf(stderr,"Warning: type mismatch between multiply defined symbols\n");
					    	fprintf(stderr,"         %s: known as %c, is now %c\n", (*found)->name, TYPE(*found), type);
						}

						override = ('U' == TYPE(*found));

						if (override) {
							TYPE(*found) = otype;
							assert( -1 != ((*found)->size = size) );
						}
					}
#endif
				}
				sym = *found;

				weak = 0;

				switch ( TOUPPER(type) ) {
bail:
					default:
						fprintf(stderr,"Unknown symbol type '%c' (line %i)\n",type,line);
					return -1;

					case 'W':
					case 'V': weak = 1;
					case 'D':
					case 'T':
					case 'B':
					case 'R':
					case 'G':
					case 'S':
					case 'A':
					case 'C':
							  {
							  Xref ex;
							  obj->nexports++;
							  assert( obj->exports = realloc(obj->exports, sizeof(*obj->exports) * obj->nexports) );
							  /* check alignment with flags */
							  assert( 0 == ((unsigned long)obj->exports & XREF_FLAGS) );
							  ex = &obj->exports[obj->nexports - 1];
							  ex->sym = sym;
							  ex->obj = obj;
							  xref_set_weak(ex,weak);
							  xref_set_next(ex,0);
							  }
					break;

					case '?':
							  if ( !force ) goto bail;
							  /* else: fall thru / less paranoia */

					case 'w': /* powerpc-rtems-gcc has, for unknown reasons, global symbols
							   * of this type in its startfiles...
							   */

					case 'U':
							  {
							  Xref im;
							  obj->nimports++;
							  assert( obj->imports = realloc(obj->imports, sizeof(*obj->imports) * obj->nimports) );
							  /* check alignment with flags */
							  assert( 0 == ((unsigned long)obj->imports & XREF_FLAGS) );
							  im = &obj->imports[obj->nimports - 1];
							  im->sym = sym;
							  im->obj = obj;
							  xref_set_weak(im,0);
							  xref_set_next(im,0);
							  }
					break;
				}
#if DEBUG & DEBUG_SCAN
				fprintf(debugf,"\t '%c' %s\n",type,buf);
#endif
			break;
		}
	}
	fixupObj(obj);
	free(nsym);
	return 0;
}

static void
gatherDanglingUndefsAct(const void *pnode, const VISIT when, const int depth)
{
const Sym sym = *(const Sym*)pnode;
	if ( (postorder == when || leaf == when) && ! sym->exportedBy) {
		Xref ex;
		undefSymPod.nexports++;
		undefSymPod.exports = realloc(undefSymPod.exports, sizeof(*undefSymPod.exports) * undefSymPod.nexports);
		/* check alignment with flags */
		assert( 0 == ((unsigned long)undefSymPod.exports & XREF_FLAGS) );
		ex = &undefSymPod.exports[undefSymPod.nexports-1];
		ex->sym  = sym;
		ex->obj  = &undefSymPod;
		xref_set_weak(ex,0);
		xref_set_next(ex,0);
	}
}


/*
 * Gather symbols which are defined nowhere and attach them
 * to the export list of the 'special' object at the list head.
 */
void
gatherDanglingUndefs()
{
	twalk(symTbl, gatherDanglingUndefsAct);
	fixupObj(&undefSymPod);
}

/*
 * Link an object and recursively resolve all of its
 * dependencies. Objects which are not already members
 * of a link set become members of 'f's link set.
 *
 * Semantics: caller must have asserted that
 * 'f' is not part of any linkset already
 * (f->link.anchor == 0).
 * Then, the caller sets f->link.anchor and
 * calls 'link' to perform a recursive link.
 */

int
linkObj(ObjF f, char *symname)
{
register int i;
register Xref imp;

	assert(f->link.anchor);

	if (verbose & DEBUG_LINK) {
		fprintf(logf,"Linking '"); printObjName(debugf,f); fputc('\'', debugf);
		if (symname)
			fprintf(logf,"because of '%s'",symname);
		fprintf(logf," to %s link set\n", f->link.anchor->name);
	}

	for (i=0, imp=f->imports; i<f->nimports; i++, imp++) {
		register Sym *found;
		assert( 0 == XREF_NEXT(imp) );
		assert (found = (Sym*)tfind( imp->sym, &symTbl, symcmp ));

		/* add ourself to the importers of that symbol */
		xref_set_next(imp, (*found)->importedFrom);
		(*found)->importedFrom = imp;

		if ( !(*found)->exportedBy ) {
			if (warn & WARN_UNDEFINED_SYMS) {
				fprintf(stderr,
					"Warning: symbol %s:%s undefined\n",
					f->name, imp->sym->name);
			}
		} else {
			ObjF	dep= (*found)->exportedBy->obj;
			if ( f->link.anchor && !dep->link.anchor ) {
				dep->link.anchor = f->link.anchor;
				linkObj(dep,(*found)->name);
			}
		}
	}

	f->link.next = (f->link.anchor->set);
	f->link.anchor->set = f;

	return 0;
}

/* Dump info about a symbol to 'feil' */
void
trackSym(FILE *feil, Sym s)
{
Xref 			ex;
Xref			imp;
DepPrintArgRec	arg;

	arg.depthIndent = -2;
	arg.file		= feil;

	fprintf(feil,"What I know about Symbol '%s':\n", s->name);
	fprintf(feil,"  Defined in object: ");
	if ( ! (ex = s->exportedBy) ) {
		fprintf(feil," NOWHERE!!!\n");
	} else {
		printObjName(feil, ex->obj);
		fprintf(feil,"%s\n", XREF_WEAK(ex) ? " (WEAK)" : "");
		while ( ex=XREF_NEXT(ex) ) {
			fprintf(feil,"      AND in object: ");
			printObjName(feil, ex->obj);
			fprintf(feil,"%s\n", XREF_WEAK(ex) ? " (WEAK)" : "");
		}
	}

	if ( (ex=s->exportedBy) ) {
		fprintf(feil,"  Depending on objects (triggers linkage of):");
		if (0 == ex->obj->nimports) {
			fprintf(feil," NONE\n");
		} else {
			fprintf(feil,"\n");
			arg.minDepth    = 1;
			arg.indent      = 0;
			depwalk(ex->obj, depPrint, (void*)&arg, WALK_IMPORTS | WALK_BUILD_LIST);
			depwalkListRelease(ex->obj);
		}
	}

	fprintf(feil,"  Objects depending (maybe indirectly) on this symbol:\n");
    fprintf(feil,"  Note: the host object may depend on yet more objects due to other symbols...\n");

	imp = s->importedFrom;

	if ( imp ) {
		fprintf(feil,"\n");
		arg.minDepth    = 0;
		arg.indent      = 4;
		do {
			depwalk(imp->obj, depPrint, (void*)&arg, WALK_EXPORTS | WALK_BUILD_LIST);
			depwalkListRelease(imp->obj);
		} while ( imp = XREF_NEXT(imp) );
	} else {
		fprintf(feil," NONE\n");
	}
}

/* Dump info about an object to 'feil' */
int
trackObj(FILE *feil, ObjF f)
{
int				i;
DepPrintArgRec	arg;

	fprintf(feil,"What I know about object '");
	printObjName(feil,f);
	fprintf(feil,"':\n");

	fprintf(feil,"  Exported symbols:\n");

	for ( i=0; i<f->nexports; i++)
		fprintf(feil,"    %s\n",f->exports[i].sym->name);

	fprintf(feil,"  Imported symbols:\n");

	for ( i=0; i<f->nimports; i++)
		fprintf(feil,"    %s\n",f->imports[i].sym->name);

	fprintf(feil,"  Objects depending on me (including indirect dependencies):\n");

	arg.minDepth    = 0;
	arg.indent      = 4;
	arg.depthIndent = -1;
	arg.file		= feil;

	depwalk(f, depPrint, (void*)&arg, WALK_EXPORTS | WALK_BUILD_LIST);
	depwalkListRelease(f);

	fprintf(feil,"  Objects I depend on (including indirect dependencies):\n");

	depwalk(f, depPrint, (void*)&arg, WALK_IMPORTS | WALK_BUILD_LIST);
	depwalkListRelease(f);
	
	return 0;
}

static void
doUnlink(ObjF f, int depth, void *closure)
{
Xref	imp,p,n;
int		i;
ObjF	*pl;

	if ( verbose & DEBUG_UNLINK ) {
		fprintf(logf,"\n  removing object '");
		printObjName(logf,f);
		fprintf(logf,"'... ");
	}

	for (i=0, imp=f->imports; i<f->nimports; i++, imp++) {
		/* remove ourself from the list of importers of that symbol */

		/* retrieve our predecessor */
		p = imp->sym->importedFrom;
		
		if ( p == imp ) {
			imp->sym->importedFrom = XREF_NEXT(imp);
		} else {
			while ( p && ((n=XREF_NEXT(p)) != imp) ) {
				p = n;
			}
			assert( p );
			xref_set_next(p, XREF_NEXT(imp));
		}
		
		xref_set_next(imp, 0);
	}

	/* remove this object from its linkset */
	for (pl = &f->link.anchor->set; *pl && (*pl != f); pl = &((*pl)->link.next) )
		/* do nothing else */;
	assert( *pl );
	*pl = f->link.next;
	f->link.next   = 0;
	f->link.anchor = 0;

	if ( verbose & DEBUG_UNLINK )
		fprintf(logf,"OK\n");
}

static void
checkSysLinkSet(ObjF f, int depth, void *closure)
{
ObjF *reject = closure;
	if ( f->link.anchor == &appLinkSet ) {
		if ( ! *reject ) {
		  	if ( verbose & DEBUG_UNLINK ) {
				fprintf(logf,"  --> rejected because '");
				printObjName(logf,f);
				fprintf(logf,"' is needed by app");
			}
		*reject = f;
		}
	}
}

static void
checkSanity(ObjF f, int depth, void *closure)
{
Xref	ex;
int		i;
	/* sanity check. All exported symbols' import lists must be empty */
	for (i=0, ex=f->exports; i<f->nexports; i++,ex++) {
		assert( ex->sym->importedFrom == 0 );
	}
}

/*
 * Remove an object and all objects depending on it
 * (i.e. the files which would trigger linkage of 'f')
 * from its link set.
 * 
 * if 'checkOnly' is nonzero, the real work is not done
 * but the return value indicates if it would succeed.
 *
 * RETURNS: 0 on success, NONZERO on failure (i.e. members
 *          of the Application link set depend on 'f').
 */

ObjF
unlinkObj(ObjF f, int checkOnly)
{
ObjF	reject = 0;
int		i;

	if ( !f->link.anchor ) {
		fputc(' ',logf);
		fputc(' ',logf);
		printObjName(logf,f);
		fprintf(logf," is currently not part of any link set.\n");
		return 0;
	}

	depwalk(f, 0, 0, WALK_EXPORTS | WALK_BUILD_LIST);

	/* check if any of the objects is part of the
	 * fundamental link set
	 */
	workListIterate(f, checkSysLinkSet, &reject);

	if ( !checkOnly ) {
		if ( ! reject ) {
			workListIterate(f, doUnlink, 0);
			workListIterate(f, checkSanity, 0);
		} else if ( verbose & DEBUG_UNLINK ) {
			fprintf(logf,"\n  skipping object '");
			printObjName(logf,f);
			fprintf(logf,"' (");
			printObjName(logf,reject);
			fprintf(logf, ":");
			for ( i = 0; i<reject->nimports; i++) {
				if (reject->imports[i].obj == f) {
					fprintf(logf," %s",reject->imports[i].sym->name);
				}
			}
			fprintf(logf,")... done.\n");
		}
	}
	depwalkListRelease(f);
	return reject;
}

/*
 * Unlink all modules depending on undefined symbols;
 * 
 * (This will fail for 'application' / critical objects.
 * The reason is that some symbols still might be
 * defined by the linker script or startup files [crti.o
 * & friends] - failure of 'unlinking' critical objects
 * is tolerated.)
 */
int
unlinkUndefs()
{
int		i;
Xref	ex,p,n;
ObjF	q = &undefSymPod;

	for (i=0, ex=q->exports; i<q->nexports; i++,ex++) {
		if ( verbose & DEBUG_UNLINK )
			fprintf(logf,"removing objects depending on '%s'...", ex->sym->name);
		for ( p = ex->sym->importedFrom; p; p=XREF_NEXT(p) ) {
			/* If any importer of this symbol rejects unlinking
			 * we probably deal with a startup file / linker script 
			 * symbol and just skip it...
			 */
			if ( unlinkObj(p->obj, 1) ) {
				if ( verbose & DEBUG_UNLINK )
					fprintf(logf," (probably a linker script / startfile symbol).\n");
				goto skipped;
			}
		}
		while (ex->sym->importedFrom && 0==unlinkObj(ex->sym->importedFrom->obj, 0))
			/* nothing else to do */;
		if (ex->sym->importedFrom) {
			/* ex->sym.importedFrom must depend on a system module, skip to the next */
			p = ex->sym->importedFrom;
			do {
				if ( verbose & DEBUG_UNLINK ) {
					fprintf(logf,"\n  skipping application dependeny; object '");
					printObjName(logf,p->obj);
					fprintf(logf,"'\n");
				}
				while ( (n=XREF_NEXT(p)) && 0 == unlinkObj(n->obj, 0) )
					/* nothing else to do */;
			} while ( p = n ); /* reached a system module; skip */
		}
		if ( verbose & DEBUG_UNLINK )
			fprintf(logf,"done.\n");
	skipped:
		continue;
	}
	return 0;
}

static DepWalkAction	depwalkAction   = 0;
static void				*depwalkClosure = 0;
static int				depwalkMode     = 0;


#define BUSY 		((ObjF)depwalk) /* just some address */
#define MATCH_ANY	((Lib)depwalk)	/* just some address */

#define DO_EXPORTS (depwalkMode & WALK_EXPORTS)

/*
 * Recursively walk the 'exports' or 'imports' list of an object
 * (private helper routine with few arguments to save stack space
 */
static void
depwalk_rec(ObjF f, int depth)
{
register int	i;
register Xref ref;

	if (depwalkAction)
		depwalkAction(f,depth,depwalkClosure);

	for ( i=0; i < (DO_EXPORTS ? f->nexports : f->nimports); i++ ) {
		for (ref = (DO_EXPORTS ? f->exports[i].sym->importedFrom : f->imports[i].sym->exportedBy);
			 ref;
			 ref = (DO_EXPORTS ? XREF_NEXT(ref) : 0 /* use only the first definition */) ) {

			assert( ref->obj != f );

			if ( !ref->obj->work ) {
				/* mark in use */
#ifdef NWORK
/*				fprintf(debugf,"Linking %s between %s and %s\n", ref->obj->name, f->name, f->work && f->work != BUSY ? f->work->name : "NIL");    */
				ref->obj->work = f->work;
				f->work        = ref->obj;
				assert( 0 == checkCircWorkList(f) );
#else
				ref->obj->work = f;
				if ( (depwalkMode & WALK_BUILD_LIST) ) {
					ref->obj->work1 = f->work1;
					f->work1	   = ref->obj;
				}
#endif
				depwalk_rec(ref->obj, depth+1);
				if ( ! (depwalkMode & WALK_BUILD_LIST) ) {
#ifdef NWORK
					f->work        = ref->obj->work;
					ref->obj->work = 0;
#else
					ref->obj->work = 0;
#endif
				}
			} /* else break circular dependency */
		}
	}
}

/*
 * Recursively walk the 'exports' or 'imports' list of an object
 * and invoke a user defined action on every visited node.
 *
 * 'mode' flags define whether to walk the 'export' or 'import' list.
 *
 * Another flag 'WALK_BUILD_LIST' instructs the routine to
 * build a linked list of all objects referenced (directly or indirectly)
 * by the export or import list. No action is invoked in this mode.
 * The list must be released by calling depwalkListRelease() which will
 * also cause the action to be invoked for every node on the list.
 */
void
depwalk(ObjF f, DepWalkAction action, void *closure, int mode)
{
	assert(f->work == 0 );

	assert( !(depwalkMode & WALK_BUILD_LIST) );

	depwalkMode    = mode;
	depwalkAction  = (depwalkMode & WALK_BUILD_LIST) ? 0 : action;
	depwalkClosure = closure;

	f->work = BUSY;
	depwalk_rec(f, 0);

	if (depwalkMode & WALK_BUILD_LIST) {
		depwalkAction = action;
	} else {
		f->work = 0;
	}
}

/* Clear 'f's work list */
void
workListRelease(ObjF f)
{
ObjF tmp;
#ifdef NWORK
	for (tmp = f; tmp && tmp!= BUSY; ) {
		tmp     = f->work;
		f->work = 0;
		f       = tmp;
	}
#else
	for (tmp = f; tmp; ) {
		tmp = f->work1;
		f->work1 = 0;
		f->work  = 0;
		f        = tmp;
	}
#endif
}

/* Invoke an action for every node on the work list */
void
workListIterate(ObjF f, DepWalkAction action, void *closure)
{
int   depth = 0;
#ifdef NWORK
	while ( f && f != BUSY ) { 
		action(f, depth++, closure);
		f = f->work;
	}
#else
	while ( f ) {
		action(f, depth++, closure);
		f = f->work1;
	}
#endif
}

/* Release the work list of an object and invoke the action
 * passed to 'depwalk()' 
 */
void
depwalkListRelease(ObjF f)
{
ObjF	tmp;
int		depth = 0;
	assert( (depwalkMode & WALK_BUILD_LIST) );
	if (depwalkAction)
		workListIterate(f, depwalkAction, depwalkClosure);
	workListRelease(f);
	depwalkMode = 0;
}


static void
symTraceAct(const void *pnode, const VISIT when, const int depth)
{
	if ( postorder == when || leaf == when ) {
		trackSym(logf, *(Sym*)pnode);
	}
}

/* Paranoia check */
int checkObjPtrs()
{
int		err=0;
ObjF	f;
	for (f=fileListHead; f; f=f->next) { 
		int ii;
		for (ii=0; ii<f->nexports; ii++) {
			if (f->exports[ii].obj != f) {
				fprintf(stderr,"%s %ith export obj pointer corrupted\n", f->name, ii);
				err++;
			}
		}
		for (ii=0; ii<f->nimports; ii++) {
			if (f->imports[ii].obj != f) {
				fprintf(stderr,"%s %ith import obj pointer corrupted\n", f->name, ii);
				err++;
			}
		}
	}
	return err;
}

typedef struct {
	ObjF	test;
	int		result;
} CheckArgRec, *CheckArg;

static void circCheckAction(ObjF f, int depth, void *closure)
{
CheckArg arg  = closure;
	if (depth > 0 && f == arg->test)
		arg->result = -1;
}

int checkCircWorkList(ObjF f)
{
CheckArgRec arg;
	arg.test   = f;
	arg.result = 0;
	workListIterate(f, circCheckAction, &arg);
	return arg.result;
}

/* Depwalk action for printing node info */
void
depPrint(ObjF f, int d, void *closure)
{
DepPrintArg  arg  = (DepPrintArg)closure;
FILE		*feil = arg->file;

	if (d < arg->minDepth)
		return;

	if (!feil)
		feil = logf;

	if (arg->depthIndent >= 0)
		d <<= arg->depthIndent;
	else
		d = 0;
	d += arg->indent;
	while (d-- > 0)
		fputc(' ', feil);
	printObjName(feil, f);
	fputc('\n', feil);
}

/*
 * Compare two objects by name. If the names match,
 * the library names are compared unless any of the
 * library pointers has the special value 'MATCH_ANY'
 */
static int
objcmp(const void *a, const void *b)
{
ObjF	obja=*(ObjF*)a;
ObjF	objb=*(ObjF*)b;
int		rval;

	if (rval = strcmp(obja->name, objb->name))
		return rval;

	if (MATCH_ANY == obja->lib  || MATCH_ANY == objb->lib)
		return 0;

	/* matching object names; compare libraries */
	if (obja->lib) {
		if (objb->lib) {
			return libcmp(obja->lib->name, objb->lib->name);
		} else
			return 1; /* a has library name, b has not b<a */
	}
	rval = objb->lib ? -1 : 0;

	return rval;
}

/* Build a sorted index of all objects */
ObjF *
fileListBuildIndex()
{
ObjF *rval;
ObjF f;
int  i;

	assert( rval = malloc(numFiles * sizeof(*rval)) );
	for ( i=0, f = fileListHead; f; i++, f=f->next) {
		rval[i] = f;
	}
	qsort(rval, numFiles, sizeof(*rval), objcmp);
	return rval;
}

/* Check for multiply define symbols in the link set of an object */
int
checkMultipleDefs(LinkSet s)
{
ObjF	f;
int		i;
Xref	r;
int		rval = 0;

	fprintf(logf,
			"Checking for multiply defined symbols in the %s link set:\n",
			s->name);

	for ( f = s->set; f; f=f->link.next ) {
		
		if ( BUSY == f->work )
			continue;

		for ( i = 0; i < f->nexports; i++ ) {
			r = f->exports[i].sym->exportedBy;


			if (XREF_NEXT(r)) {
				int isCommon = 0;
#ifdef TYPE
				isCommon = 'C' == TOUPPER(TYPE(f->exports[i].sym));
#endif

				if ( !isCommon) {
					rval++;
					fprintf(logf,"WARNING: Name Clash Detected; symbol '%s'"
#ifdef TYPE
					   " (type '%c')"
#endif
					   " exported by multiple objects:\n",
						f->exports[i].sym->name
#ifdef TYPE
						, TYPE(f->exports[i].sym)
#endif
					);
				}
				while (r) {
					if (!isCommon) {
						fprintf(logf,"  in '");
						printObjName(logf,r->obj);
						fprintf(logf,"'%s\n", XREF_WEAK(r) ? " (WEAK [not implemented yet])" : "");
					}
					r->obj->work = BUSY;
					r = XREF_NEXT(r);
				}
			}
		}
	}

	for ( f = fileListHead; f; f=f->next )
		f->work = 0;

	fprintf(logf,"OK\n");

	return rval;
}

/*
 * Find an object with 'name' in the index of all objects.
 *
 * Return the number of matches found.
 * A pointer to the first match in the index
 * array is returned in *pfound.
 *
 * 'name' is temporarily edited.
 */
int
fileListFind(char *name, ObjF **pfound)
{
char	*objn, *po, *pc;
ObjFRec	frec = {0};
ObjF	f    = &frec;
Lib		l;
ObjF	*found, *end;
int		rval = 0;

	if ( ! (objn=splitName(name,&po,&pc)) ) {
		return 0; /* ill-formed name */
	}

	f->name = objn;

	if (po && *name) {
		for (l=libListHead; l; l=l->next) {
			if ( !libcmp(l->name, name) ) {
				break;
			}
		}
		if (!l) {
			goto cleanup;
		}
		f->lib = l;
	} else  {
		f->lib = MATCH_ANY;
	}

	if ( ! (found = bsearch(&f, fileListIndex, numFiles, sizeof(f), objcmp)) )
		goto cleanup;

	/* find all matches */
	end = found;
	while ( ++end < fileListIndex + numFiles && !objcmp(&f,end) )
		/* nothing else to do */;

	while (--found >= fileListIndex && !objcmp(&f, found))
		/* nothing else to do */;

	found++;

	if (pfound)
		*pfound = found;

	rval = end-found;

cleanup:
	if (po) {
		*po = '[';
		*pc = ']';
	}
	return rval;
}

/* My private versions of getc, ungetc. They operate
 * on a string buffer (*pchpt) until it's empty and then
 * switch to the stream 'f'.
 * These routines are mainly used to skip comments in
 * the 'remove_list' file.
 */
static int mygc(char **pchpt, FILE *f)
{
int rval;
	if (*pchpt) {
		rval = *((*pchpt)++);
		if (!**pchpt)
			*pchpt = 0;
	} else {
		rval = getc(f);
	}
	return rval;
}

static int myugc(int ch, char **pchpt, FILE *f)
{
	return !*pchpt ? ungetc(ch,f) : (*(--(*pchpt)) = (unsigned char)ch);
}

/*
 * (Un)link all files listed in the file 'fname' along with
 * objects depending on them.
 *
 * RETURNS: 0 on success,
 *          NONZERO on failure:
 *             -4 list file not found
 *             -3 listed object member name needs more qualification
 *             -2 listed object not found
 *             -1 listed object member of Application link set
 *                (this error is ignored 
 *             These errors may be ignored by setting the sloppyness
 *             (makes probably only sense for sloppy < 2)
 */
int
processFile(ProcTab pt, int sloppy)
{
FILE *remf;
char buf[MAXBUF+1];
int  got,i;
int  line;
ObjF *pobj;
int  rval = 0;
char *comment;

	buf[MAXBUF] = 'X'; /* tag end of buffer */

	if ( ! (remf=ffind(pt->fname)) ) {
		fprintf(stderr,	"Opening %s_list file '%s': %s\n",
						pt->linkNotUnlink  ? "optional" : "exclude",
						pt->fname,
						strerror(errno));
		return -4;
	}

	fprintf(logf,
			"Processing list of files ('%s') to %s %s link set\n",
			pt->fname,
			pt->linkNotUnlink ? "add to" : "remove from",
			optionalLinkSet.name);

	line = 0;
	while ( (rval >= 0 || sloppy) && fgets(buf, MAXBUF+1, remf) ) {
		line++;

		if (!buf[MAXBUF]) {
			fprintf(stderr,"Buffer overflow in %s (line %i)\n",
							pt->fname,
							line);
			fclose(remf);
			return -5;
		}

		/* does a comment start on this line
		 * (note: '/' '*' must be on the same line, i.e. in buf)
		 */
		if ( (comment = strchr(buf, '/')) && '*'==*(comment + 1) ) {
			/* strip comment off 'buf' */
			*comment = 0;
			/* scan past comment */
			comment += 2;
			do {
				while ( '*' != mygc(&comment, remf) )
					/* nothing else to do */;
				/* at this point, we scanned past the next '*' */

				while ( '*' == (i = mygc(&comment, remf)) )
					/* nothing else to do */;

				/* at this point, we scanned past the last '*' in a row */

			} while ( '/' != i );

		}

		/* now proceed scanning the buffer */

		/* strip off white space */
		for (comment = buf; *comment && !isspace(*comment); comment++)
			/* that's it */;
		*comment = 0;

		if ( (i=strlen(buf)) < 1 )
			continue; /* left an empty buffer */

#if DEBUG & DEBUG_COMMENT
fprintf(debugf,"Scanned '%s'\n",buf); continue;
#endif

		/*
		 * ignore lines not terminating with a ':'; thus we
		 * can process ordinary name lists :-)
		 */

		if ( ':' != buf[--i] ) {
			continue;
		}
		/* strip ':' */
		buf[i] = 0;

		got = fileListFind(buf, &pobj);

		rval = sloppy;

		if ( 0 == got ) {
			char *fmt = "Object '%s' not found!\n";
			if ( (rval -= 2) >=0 ) {
				if ( ! (verbose & DEBUG_UNLINK) ) {
					/* We didn't log so far and the stderr message may
				 	* get lost...
				 	*/
					fprintf(logf, fmt, buf);
				}
				fprintf(stderr,"Warning: ");
			}
			fprintf(stderr, fmt, buf);
		} else if ( got > 1 ) {
			rval -= 3;
			fprintf(stderr,"Multiple occurrences of '%s':\n",buf);
			for (i=0; i<got; i++) {
				fputc(' ',stderr);
				fputc(' ',stderr);
				printObjName(stderr,pobj[i]);
				fputc('\n',stderr);
			}
			fprintf(stderr,"please be more specific!\n",buf);
		} else  {
			if ( pt->linkNotUnlink ) {
				if ( 0 == (*pobj)->link.anchor ) {
					(*pobj)->link.anchor = &optionalLinkSet;
					sprintf(buf,"<SCRIPT>'%s'",pt->fname);
					rval -= linkObj( *pobj, buf );
				}
			} else if ( unlinkObj(*pobj, 0) ) {
				char *fmt = "Object '%s' couldn't be removed; probably it's needed by the application\n";
				if ( (rval -= 1) >= 0 ) {
					if ( ! (verbose & DEBUG_UNLINK) ) {
						/* We didn't log so far and the stderr message may
					 	* get lost...
					 	*/
						fprintf(logf, fmt, buf);
					}
					fprintf(stderr,"Warning: ");
				}
				fprintf(stderr, fmt, buf);
			}
		}
	}

	fclose(remf);
	return rval;
}

/*
 * Write EXTERN declarations for all members of a link set
 * to 'feil'. A 'title' may be added as a C-style comment.
 *
 * The output is suitable for GNU ld.
 */
static int
writeLinkSet(FILE *feil, LinkSet s, char *title)
{
ObjF	f = s->set;
int		n;

	if ( !f )
		return 0;

	if (title)
		fprintf(feil,"/* ----- %s Link Set ----- */\n\n", title);

	for ( ; f; f = f->link.next ) {
		fprintf(feil,"/* "); printObjName(feil,f); fprintf(feil,": */\n");
		for ( n = 0; n < f->nexports; n++ ) {
			fprintf(feil,"EXTERN( %s ) /* size %i */\n", f->exports[n].sym->name, f->exports[n].sym->size);
		}
	}
	return 0;
}

/*
 * Generate a linker script with external references to enforce linking the
 * application and optional link sets
 */
int
writeScript(FILE *feil, int optionalOnly)
{
	if ( !optionalOnly ) {
		writeLinkSet(feil, &appLinkSet, "Application");
		fputc('\n',feil);
	}

	writeLinkSet(feil, &optionalLinkSet, "Optional");
	return 0;
}

static const char *getsname(Sym ps, char **pstripped)
{
char *chpt;
const char *sname = ps->name;

	if ( pstripped ) {
		*pstripped = strdup(sname);
#ifdef LINKER_VERSION_SEPARATOR
		if ( chpt = strchr(*pstripped, LINKER_VERSION_SEPARATOR) ) {
			*chpt = 0;
		}
#endif
	}
	return sname;
}

/*
 * Write symbol definitions in C source form for all members of a link set
 * to 'feil'. A 'title' may be added as a C-style comment.
 *
 * The output is suitable for building into CEXP applications
 */
static int
writeSymdefs(FILE *feil, LinkSet s, char *title, int pass)
{
ObjF	f = s->set;
int		n;
int     i;

	if ( !f )
		return 0;

	if (title)
		fprintf(feil,"/* ----- %s Link Set ----- */\n\n", title);

if ( 0 == pass ) {
	for ( i=0 ; f; f = f->link.next ) {
		fprintf(feil,"/* "); printObjName(feil,f); fprintf(feil,": */\n");
		for ( n = 0; n < f->nexports; n++ ) {
			char *stripped;
			getsname(f->exports[n].sym, &stripped);
			fprintf(feil,"extern int "DUMMY_ALIAS_PREFIX"%s%i;\n",title,i);
			fprintf(feil,"asm(\".set "DUMMY_ALIAS_PREFIX"%s%i,%s\\n\");\n",title,i,stripped);
			free(stripped);
			i++;
		}
	}
} else {
	for ( i=0 ; f; f = f->link.next ) {
		fprintf(feil,"/* "); printObjName(feil,f); fprintf(feil,": */\n");
		for ( n = 0; n < f->nexports; n++ ) {
			Sym  s      = f->exports[n].sym;
			const char *sname = getsname(s,0);
			char t      = TYPE(s);
			char ut     = toupper(t);
			fprintf(feil,"\t{\n");
			fprintf(feil,"\t\t.name        = \"%s\",\n", sname);
			fprintf(feil,"\t\t.value   =(unsigned long)&"DUMMY_ALIAS_PREFIX"%s%i,\n",title,i);
			/*fprintf(feil,"\t\t.value.ptv   =(void*)&"DUMMY_ALIAS_PREFIX"%s%i,\n",title,i);
			fprintf(feil,"\t\t.value.type  =%s,\n",      'T'==t ? "TFuncP" : "TVoid");
			fprintf(feil,"\t\t.size        =%i,\n",      s->size);
			fprintf(feil,"\t\t.flags       =0");
				if ( isupper(t) )
					fprintf(feil,"|CEXP_SYMFLG_GLBL");
				if ( ( 'W' == ut || 'V' == ut ) &&
				     strcmp("cexpSystemSymbols",sname) )
					fprintf(feil,"|CEXP_SYMFLG_WEAK");
			fprintf(feil,",\n");*/
			fprintf(feil,"\t},\n");
			i++;
		}
	}
}
	return 0;
}


/*
 * Generate a Cexp symbol table source file. Adding this to the application
 * automatically triggers linkage of the optional link sets.
 */
int
writeSource(FILE *feil, int optionalOnly)
{
int pass;
	for ( pass = 0; pass < 2; pass++ ) {
		if ( 0== pass )
			fprintf(feil,"#include \"sym.h\"\n");
		else
			fprintf(feil,"\n\nstatic struct dl_symbol systemSymbols[] = {\n");
		if ( !optionalOnly ) {
			writeSymdefs(feil, &appLinkSet, "Application", pass);
			fputc('\n',feil);
		}

		writeSymdefs(feil, &optionalLinkSet, "Optional", pass);
		if ( 1 == pass ) {
			fprintf(feil,"\t{\n");
			fprintf(feil,"\t0, /* terminating record */\n");
			fprintf(feil,"\t},\n");
			fprintf(feil,"};\n");
			fprintf(feil,"struct dl_symbl *cexpSystemSymbols = systemSymbols;\n");
		}
}
	return 0;
}


static void 
usage(char *nm)
{
char *strip = strrchr(nm,'/');
	if (strip)
		nm = strip+1;
	fprintf(stderr,"\nUsage: %s [-Odfhilmqsu] [-A main_symbol] [-L path] [-o optional_list] [-x exclude_list] [-e script_file] [-C src_file] nm_files\n\n", nm);
	fprintf(stderr,"   Object file dependency analysis; the input files must be\n");
	fprintf(stderr,"   created with 'nm -g -fposix'.\n\n");
	fprintf(stderr,"(This is ldep $Revision: 1.31 $ by Till Straumann <strauman@slac.stanford.edu>)\n\n");
	fprintf(stderr,"   Input:\n");
	fprintf(stderr,"           If no 'nm_files' are given, 'stdin' is used. The first 'nm_file' is\n");
	fprintf(stderr,"           special (unless using -A, see below): it lists MANDATORY objects/symbols\n");
    fprintf(stderr,"           (AKA 'application files').\n");
	fprintf(stderr,"           objects added by the other 'nm_files' are 'optional' unless a mandatory\n");
	fprintf(stderr,"           object depends on an optional object. In this case, the latter becomes\n");
	fprintf(stderr,"           mandatory as well.\n\n");

	fprintf(stderr,"   Options:\n");
	fprintf(stderr,"     -A:   use 'main_symbol' to construct the set of mandatory objects - anything\n");
	fprintf(stderr,"           (directly or indirectly) needed by the object defining 'main_symbol' is\n");
	fprintf(stderr,"           mandatory.\n");
	fprintf(stderr,"           NOTE: The first 'nm_file' is NOT treated special if this option is used.\n");
	fprintf(stderr,"     -F:   tolerate/ignore failure when processing 'exclude_lists'\n");
	fprintf(stderr,"     -L:   add 'path' to search path for 'nm_files', 'optional_lists' and 'exclude_lists'\n");
	fprintf(stderr,"           NOTE: if at least one '-L' is present, '.' must explicitely added.'\n");
	fprintf(stderr,"     -O:   when generating a script (see -e), only list the optional link set\n");
	fprintf(stderr,"     -o:   add a list of optional objects to the link - name them, one per line, in\n");
    fprintf(stderr,"           the file 'optional_list'. Object names must be appended a ':', e.g. 'blah.o:'!\n");
	fprintf(stderr,"           NOTES: - multiple '-o' and '-x' options may be present. The respective files\n");
	fprintf(stderr,"                    are processed in the order the options are given on the command line\n");
	fprintf(stderr,"                  - if at least one '-o' option is present, the 'optional nm_files' are\n");
	fprintf(stderr,"                    merely added to the database but NOT linked/added to the application,\n");
	fprintf(stderr,"                    ONLY objects listed in '-o' files are.\n");
	fprintf(stderr,"     -d:   show all module dependencies (huge amounts of data! -- use '-l', '-u')\n");
	fprintf(stderr,"     -e:   on success, generate a linker script 'script_file' with EXTERN statements\n");
	fprintf(stderr,"     -C:   on success, generate a C-source file with CEXP symbol table definitions\n");
	fprintf(stderr,"     -f:   be less paranoid when scanning symbols: accept 'local symbols' (map all\n");
	fprintf(stderr,"           types to upper-case) and assume unrecognized symbol types ('?') are 'U'\n");
	fprintf(stderr,"     -h:   print this message.\n");
	fprintf(stderr,"     -i:   enter interactive mode\n");
	fprintf(stderr,"     -l:   log info about the linking process\n");
	fprintf(stderr,"     -m:   check for symbols defined in multiple files\n");
	fprintf(stderr,"     -q:   quiet; just build database and do basic checks\n");
	fprintf(stderr,"     -x:   exclude/remove a list of objects from the link - name them, one per line, in\n");
	fprintf(stderr,"           the file 'exclude_list'\n");
	fprintf(stderr,"           NOTES: - if a mandatory object depends on an object to be removed, removal\n");
	fprintf(stderr,"                    is rejected.\n");
	fprintf(stderr,"                  - object names must be appended a ':', e.g. 'blah.o:' - other lines are\n");
	fprintf(stderr,"                    ignored. Thus, output from 'nm -fposix' is accepted.\n");
	fprintf(stderr,"     -s:   show all symbol info (huge amounts of data! -- use '-l', '-u')\n");
	fprintf(stderr,"     -u:   log info about the unlinking process\n");
	fprintf(stderr,"\n"
				   "   NOTES:\n");
	fprintf(stderr,"\n"
				   "     -     '-o' and '-x' files may contain 'C-style' comments ('/* */') with the restriction\n");
	fprintf(stderr,"           that a comment must not split an input line (but a comment may start on one line\n");
	fprintf(stderr,"           and end on another line, i.e.\n");
	fprintf(stderr,"               'blah/* is */illegal:'\n");
	fprintf(stderr,"               'blah:/* is \n");
	fprintf(stderr,"                legal*/foo:\n");
}

/* Primitive interactive command interpreter (database queries) */
int
interactive(FILE *feil)
{
Sym		*found;
ObjF	*f;
char	buf[MAXBUF+1];
int		len, nf, i, choice;
SymRec	sym = {0};

	sym.name = buf;

	buf[0]=0;

	buf[MAXBUF]='0';

	do {
		if ( MAXBUF == (len=strlen(buf)) ) {
			fprintf(stderr,"Line buffer overflow in interactive mode\n");
			return -1;
		}

		if ( !*buf || '\n'==*buf ) {
			fputc('\n',feil);
			fprintf(feil, "Query database (enter single '.' to quit) for\n");
			fprintf(feil, " A) Symbols, e.g. 'printf'\n");
			fprintf(feil, " B) Objects, e.g. '[printf.o]', 'libc.a[printf.o]'\n\n");
		} else {
			buf[--len]=0; /* strip trailing '\n' */
			if ( ']' == buf[len-1] ) {
				nf = fileListFind(buf, &f);

				if ( !nf ) {
					fprintf(feil,"object '%s' not found, try again.\n", buf);
				} else {
					choice = 0;
					if ( nf > 1 ) {
						fprintf(feil,"multiple instances found, make a choice:\n");
						for (i = 0; i<nf; i++) {
							fprintf(feil,"%i) - ",i); printObjName(feil, f[i]); fputc('\n', feil);
						}

						while (    !fgets( buf, MAXBUF, stdin) ||
								 1 !=sscanf(buf,"%i",&choice)  ||
								 choice < 0                    ||
								 choice >= nf ) {

								if ( !strcmp(".\n",buf) )
									return 0;

								fprintf(feil, "\nInvalid Choice, ");
								
								if (!*buf) {
									fprintf(feil,"bailing out\n");
									return -1;
								}

								fprintf(feil,"try again\n");
								*buf = 0;
						}


					}
					trackObj(feil, f[choice]);
				}
			} else {
				found = (Sym*) tfind(&sym, &symTbl, symcmp);

				if ( !found ) {
					fprintf(feil,"Symbol '%s' not found, try again\n", sym.name);
				} else {
					trackSym(feil, *found);
				}
			}
		}
	} while ( fgets(buf, MAXBUF, stdin) && *buf && strcmp(buf,".\n") );
}

#define OPT_SHOW_DEPS		(1<<0)
#define OPT_SHOW_SYMS		(1<<1)
#define OPT_INTERACTIVE		(1<<2)
#define OPT_QUIET			(1<<3)
#define OPT_MULTIDEFS		(1<<4)
#define OPT_NO_APPSET		(1<<5)
#define OPT_SLOPPY_UNLINK	(1<<6)

int
main(int argc, char **argv)
{
FILE	*feil         = stdin;
FILE	*scrf         = 0;
char	*scrn         = 0;
char    *srcn         = 0;
ObjF	lastAppObj    = 0; 
SymRec	mainSym       = {0};
ProcTab	procTab		  = 0;
int		nProc         = 0;
char	*mainName     = 0;
int		options       = 0;
int		hasOptional   = 0;
int		i,nfile,ch;
ObjF	f;
LinkSet	linkSet;
Sym		*found;

	logf = stdout;

	while ( (ch=getopt(argc, argv, "OC:FL:A:qhifsdmlux:o:e:")) >= 0 ) {
		switch (ch) { 
			default: fprintf(stderr, "Unknown option '%c'\n",ch);
					 exit(1);

			case 'h':
				usage(argv[0]);
				exit(0);


			case 'L': searchPaths = realloc(searchPaths, sizeof(*searchPaths) * (numSearchPaths + 1));
			          searchPaths[numSearchPaths++] = optarg;
			break;
			case 'A': mainSym.name = optarg;
			break;
			case 'l': verbose |= DEBUG_LINK;
			break;
			case 'O': options |= OPT_NO_APPSET;
			break;    
			case 'u': verbose |= DEBUG_UNLINK;
			break;
			case 'd': options |= OPT_SHOW_DEPS;
			break;
			case 'f': force        = 1;
			break;
			case 'i': options |= OPT_INTERACTIVE;
			break;
			case 's': options |= OPT_SHOW_SYMS;
			break;
			case 'q': options |= OPT_QUIET;
			break;
			case 'F': options |= OPT_SLOPPY_UNLINK;
			break;
			case 'o': procTab = realloc(procTab, sizeof(*procTab) * (nProc+1));
					  procTab[nProc].fname         = optarg;
					  procTab[nProc].linkNotUnlink = 1;
					  nProc++;
					  hasOptional++;
			break;
			case 'x': procTab = realloc(procTab, sizeof(*procTab) * (nProc+1));
					  procTab[nProc].fname         = optarg;
					  procTab[nProc].linkNotUnlink = 0;
					  nProc++;
			break;
			case 'm': options |= OPT_MULTIDEFS;
			break;
			case 'e': scrn = optarg;
			break;
			case 'C': srcn = optarg;
			break;
		}
	}

	debugf = logf;

	nfile = optind;

	if ( 0 == numSearchPaths ) {
		/* implicit default */
		searchPaths = defPaths;
		numSearchPaths = sizeof(defPaths) / sizeof(defPaths[0]);
	}
	for ( i = 0; i<numSearchPaths; i++ ) {
		if ( (ch = strlen(searchPaths[i])) > maxPathLen )
			maxPathLen = ch;
	}

	do {
		char *nm = nfile < argc ? argv[nfile] : "<stdin>";
		if ( nfile < argc && !(feil=ffind(nm)) ) {
			fprintf(stderr,"Opening nm_file '%s': %s\n", nm, strerror(errno));
			exit(1);
		}
		if (scan_file(feil,nm)) {
			fprintf(stderr,"Error scanning %s\n",nm); 
			exit(1);
		}
		/* the first file we scan contains the application's
		 * mandatory file set - unless '-A' is used.
		 */
		if ( !lastAppObj )
			lastAppObj = fileListTail;
	} while (++nfile < argc);

	gatherDanglingUndefs();

	fileListIndex = fileListBuildIndex();

	fprintf(logf,"Looking for UNDEFINED symbols:\n");
	for (i=0; i<fileListHead->nexports; i++) {
#if 0
		trackSym(logf, fileListHead->exports[i].sym);
#else
		fprintf(logf," - '%s'\n",fileListHead->exports[i].sym->name);
#endif
	}
	fprintf(logf,"done\n");

	assert( 0 == checkObjPtrs() );

	linkSet = &appLinkSet;

	if ( mainSym.name ) {
		if ( !(found = (Sym*)tfind( &mainSym, &symTbl, symcmp )) ) {
			fprintf(stderr,"Error: unable to find main symbol '%s'\n",mainSym.name);
			exit(1);
		}

		if ( !(*found)->exportedBy ) {
			fprintf(stderr,"Error: no object defines main application symbol '%s'\n",mainSym.name);
		}

		f = (*found)->exportedBy->obj;

		fprintf(logf,"Main application symbol '%s' found in '");
		printObjName(logf,f);
		fprintf(logf,"'; linking...\n");

		assert( !f->link.anchor );
		f->link.anchor = linkSet;
		linkObj(f, mainSym.name);
		linkSet = hasOptional ? 0 : &optionalLinkSet;

		/* ignore lastAppObj */
	}

	for ( f=fileListFirst(); f && linkSet; f=f->next) {
		if (!f->link.anchor) {
			f->link.anchor = linkSet;
			linkObj(f, 0);
		}
		if ( f==lastAppObj )
			linkSet = hasOptional ? 0 : &optionalLinkSet;	
	}

	if ( options & OPT_QUIET ) {
		fprintf(logf,"OK, that's it for now\n");
		exit(0);
	}

	for ( i=0; i<nProc; i++ ) {
#define F_SLOPPYNESS	1
		/* tolerate failure to unlink due to dependency on app link set */
		if (F_SLOPPYNESS +
			processFile( &procTab[i],
						(options & OPT_SLOPPY_UNLINK) ? F_SLOPPYNESS : 0) < 0 )
			exit(1);
	}


	if ( options & OPT_SHOW_SYMS )
		twalk(symTbl, symTraceAct);

	if ( options & OPT_SHOW_DEPS ) {
			for (f=fileListFirst(); f; f=f->next) {
			DepPrintArgRec arg;
				arg.minDepth    =  0;
				arg.indent      = -4;
				arg.depthIndent = 2;
				arg.file		= logf;
#if 0
			/* this recursion can become VERY deep */
			fprintf(logf,"\n\nDependencies ON object: ");
			depwalk(f, depPrint, (void*)&arg, WALK_EXPORTS);
#endif
			fprintf(logf,"\nFlat dependency list for objects requiring: %s\n", f->name);
			arg.indent      = 0;
			arg.depthIndent = -1;
			depwalk(f, depPrint, (void*)&arg, WALK_EXPORTS | WALK_BUILD_LIST);
			depwalkListRelease(f);
		}
	}

	fprintf(logf,"Removing undefined symbols\n");
		unlinkUndefs();


	if ( options & OPT_MULTIDEFS ) {
		checkMultipleDefs(&appLinkSet);
		checkMultipleDefs(&optionalLinkSet);
	}

	if ( options & OPT_INTERACTIVE ) {
		interactive(stderr);
	}

	assert( 0 == checkObjPtrs() );

	if ( scrn ) {
		fprintf(logf,"Writing linker script to '%s'...", scrn);
		if ( !(scrf = fopen(scrn,"w")) ) {
			perror("opening script file");
			fprintf(logf,"opening file failed.\n");
			exit (1);
		}
		writeScript(scrf, options & OPT_NO_APPSET);
		fclose(scrf);
		fprintf(logf,"done.\n");
	}
	if ( srcn ) {
		fprintf(logf,"Writing CEXP symbol table source file to '%s'...", srcn);
		if ( !(scrf = fopen(srcn,"w")) ) {
			perror("opening source file");
			fprintf(logf,"opening file failed.\n");
			exit (1);
		}
		writeSource(scrf, options & OPT_NO_APPSET);
		fclose(scrf);
		fprintf(logf,"done.\n");
	}

	free(procTab);

	return 0;
}
