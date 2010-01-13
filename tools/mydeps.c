#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define ONCEMORE 1024

struct namerec {
	char *name;
};
/* Cexp private versions of getc, ungetc. They operate
 * on a string buffer (*pchpt) until it's empty and then
 * switch to the stream 'f'.
 * These routines are mainly used to skip comments in
 * the 'remove_list' file.
 */
static int 
mygc(char **pchpt, FILE *f)
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

static int 
find_duplicate(struct namerec *rec, int count, char *name)
{
	struct namerec *p;

	for (p = rec + count - 1; p >= rec; --p) {
		if (!strcmp(p->name, name))
			return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int i, vecsize, count = 0;
	FILE *fout, *fin;
	char *comment, buf[64];
	struct namerec *vec;
	
	fin = fopen(argv[1], "r");
	if (!fin) {
		perror("opening config file");
		return -1;
	}
	
	fout = fopen(argv[2], "w");
	if (!fout) {
		perror("opening source file");
		return -1;
	}

	vec = malloc(ONCEMORE*sizeof(struct namerec));
	if (!vec) {
		printf("no memory\n");
		fclose(fin);
		fclose(fout);
		return -1;
	}
	vecsize = ONCEMORE;
	
	fprintf(fout,"#include \"sym.h\"\n\n");

	while (fgets(buf, sizeof(buf), fin)) {

		if ( !buf[63] ) {
			fprintf(stderr,"Scanner buffer overrun\n");
			return -1;
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
				while ( '*' != mygc(&comment, fin) )
					/* nothing else to do */;
				/* at this point, we scanned past the next '*' */

				while ( '*' == (i = mygc(&comment, fin)) )
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
printf("Scanned '%s'\n",buf); continue;
#endif
		if (find_duplicate(vec, count, buf))
			continue;

		fprintf(fout,"extern int %s;\n",buf);
		vec[count++].name = strdup(buf);
		if (count > vecsize) {
			vecsize += ONCEMORE;
			vec = realloc(vec, vecsize*sizeof(struct namerec));
		}
	}

	fprintf(fout,"\nstatic struct dl_symbol system_symbols[] = {\n");

	for(i = 0; i < count; i++) {
		fprintf(fout,"\t{\n");
		fprintf(fout,"\t\t.name   = \"%s\",\n", vec[i].name);
		fprintf(fout,"\t\t.value  = (unsigned long)&%s,\n", vec[i].name);
		fprintf(fout,"\t},\n");
	}

	fprintf(fout,"\t{\n");
	fprintf(fout,"\t0, /* terminating record */\n");
	fprintf(fout,"\t},\n");
	fprintf(fout,"};\n");
	fprintf(fout,"struct dl_symbl *cexpSystemSymbols = system_symbols;\n");

	return 0;
}
