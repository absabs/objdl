#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

struct list {
	char *name;
	struct list *next;
};
/* Cexp private versions of getc, ungetc. They operate
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

int
main(int argc, char *argv[])
{
	int i;
	FILE *fout, *fin;
	char *comment, buf[64];
	struct list *p, head;
	
	p = &head;	
	fin = fopen(argv[1], "r");
	if (!fin) {
		perror("opening config file");
		exit (1);
	}
	
	fout = fopen(argv[2], "w");
	if (!fout) {
		perror("opening source file");
		exit (1);
	}
	
	fprintf(fout,"#include \"cexpsyms.h\"\n\n");

	while (fgets(buf, sizeof(buf), fin)) {
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

		fprintf(fout,"extern int %s;\n",buf);
		p->next = malloc(sizeof(*p));
		p->next->name = strdup(buf);
		p = p->next;

	}
	p->next = NULL;

	fprintf(fout,"\nstruct rec system_symbols[] = {\n");

	for(p = head.next; p; p = p->next) {
		fprintf(fout,"\t{\n");
		fprintf(fout,"\t\t.name   = \"%s\",\n", p->name);
		fprintf(fout,"\t\t.value  = (unsigned long)&%s,\n", p->name);
		fprintf(fout,"\t},\n");
	}

	fprintf(fout,"\t{\n");
	fprintf(fout,"\t0, /* terminating record */\n");
	fprintf(fout,"\t},\n");
	fprintf(fout,"};\n");
}
