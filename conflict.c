#ifndef	lint
static	char	sccs_id[] = "@(#)conflict.c	1.4 88/08/15 07:55:10";
#endif	lint

/*
 * Title:	conflict.c (path-conflict display)
 * Author:	T.E.Dickey
 * Created:	15 Apr 1988
 * Modified:
 *		11 Aug 1988, port to apollo sys5 environment
 *
 * Function:	Reports pathname conflicts by making a list of the directories
 *		which are listed in the environment variable PATH, and then
 *		scanning these directories for executable files.  If arguments
 *		given to this program, the test for executable files is limited
 *		to the given names (after stripping directory-prefix).
 *
 *		The report is sent to standard output and always shows the
 *		list of directories.  Conflicts are reported on succeeding
 *		lines.
 *
 * patch:	The number of directories which are searched is limited to the
 *		number of bits in a long.
 */

#define		DIR_PTYPES	/* include directory-definitions */
#include	"ptypes.h"
extern	int	optind;		/* index in 'argv[]' of first argument */
extern	char	**vecalloc();
extern	char	*getcwd(),
		*getenv(),
		*strrchr(),
		*stralloc(),
		*strncpy();

typedef	struct	{
		char	*name;	/* name of executable file */
		long	mask;	/* relative position in path, bit-vector */
	} INPATH;

#define	def_doalloc	INPATH_alloc
	/*ARGSUSED*/
	def_DOALLOC(INPATH)

static	INPATH	*inpath;
static	char	*pathlist;
static	char	dot[BUFSIZ];
static	unsigned total;
static	int	width,
		l_opt,		/* long report: shows all items */
		v_opt,		/* verbose */
		my_uid, my_gid, root;

compar(p1, p2)
INPATH	*p1, *p2;
{
	return (strcmp(p1->name, p2->name));
}

showmask(len, mask)
long	mask;
{
register int j, c = 0, d;

	for (j = 0; j < len; j++) {
		if (mask & (1L << j)) {
			if (c)
				c = '+';
			else
				c = '*';
			d = c;
		} else
			d = '-';	
		PRINTF("%c", d);
	}
}

conflict(path, mask, argc, argv)
char	*path;
long	mask;
char	*argv[];
{
DIR		*dp;
struct	direct	*de;
struct	stat	sb;
register int	j;

	if (dp = opendir(path)) {
		(void)chdir(path);

		while (de = readdir(dp)) {
		register
		int	ok	= FALSE,
			found	= FALSE;
		char	*s;

			/* If arguments are given, restrict search to them */
			if (argc > 1) {
				for (j = optind; j < argc; j++) {
					if (s = strrchr(argv[j], '/'))
						s++;
					else
						s = argv[j];
					if (!strcmp(s, de->d_name)) {
						found = TRUE;
						break;
					}
				}
				if (!found)	continue;
			}

			/* Verify that the name is a file, and executable */
			if (stat(de->d_name, &sb) < 0)
				continue;
			if ((sb.st_mode & S_IFMT) != S_IFREG)
				continue;

#define	isEXEC(n)	(sb.st_mode & (S_IEXEC >> n))

			if (isEXEC(6))		/* world-executable */
				ok = TRUE;
			else if ((root || (my_gid == sb.st_gid))
			&&	isEXEC(3))	/* group-executable */
				ok = TRUE;
			else if ((root || (my_uid == sb.st_uid))
			&&	isEXEC(0))	/* owner-executable */
				ok = TRUE;
			if (!ok)
				continue;

			/* Find the name in our array of all names */
			found	= FALSE;
			for (j = 0; j < total; j++) {
				if (!strcmp(inpath[j].name, de->d_name)) {
					inpath[j].mask |= mask;
					found = TRUE;
					break;
				}
			}

			/* If not there, add it */
			if (!found) {
				j = total++;
				inpath = DOALLOC(inpath, INPATH, total);
				inpath[j].name = stralloc(de->d_name);
				inpath[j].mask = mask;
			}
			if (v_opt)
				PRINTF("%c %s/%s\n",
					found ? '+' : '*',
					path, de->d_name);
		}
		closedir(dp);
		(void)chdir(dot);
	}
}

main(argc,argv)
char	*argv[];
{
long	mask;
register
int	found, j, k;
char	*s, *t,
	**vec,
	bfr[BUFSIZ];

	while ((j = getopt(argc, argv, "lv")) != EOF) switch (j) {
	case 'l':	l_opt = TRUE;	break;
	case 'v':	v_opt = TRUE;	break;
	default:	PRINTF("usage: conflict [-l] [-v] [list_of_files]\n");
			(void)exit(0);
			/*NOTREACHED*/
	}

	(void)getcwd(dot, sizeof(dot)-2);
	PRINTF("Current working directory is \"%s\"\n", dot);

	pathlist = stralloc(getenv("PATH"));
	for (s = pathlist, j = 0; *s; s++)
		if (*s == ':')
			j++;
	vec = vecalloc((unsigned)(++j));

	my_uid = getuid();
	my_gid = getgid();
	root   = (my_uid == 0);

	for (s = pathlist, j = 0; *s; s++) {
		for (t = s; *t; t++)
			if (*t == ':')
				break;
		if (s == t)
			continue;
		strncpy(bfr, s, t-s)[t-s] = EOS;
		showmask(width = j+1, 0L);
		PRINTF("> %s\n", bfr);
		abspath(bfr);
		s = t;
		found = FALSE;
		for (k = 0; k < j; k++) {
			if (!strcmp(bfr, vec[j])) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vec[j] = stralloc(bfr);
			conflict(vec[j], mask = (1L << j), argc, argv);
			j++;
		}
	}

	if (total != 0) {
		qsort((char *)inpath, (LEN_QSORT)total, sizeof(INPATH), compar);
		for (j = 0; j < total; j++) {
			/*
			 * If "-l" is not selected, reject items which do not
			 * show a conflict.
			 */
			found = FALSE;
			if (!l_opt) {
				for (k = 0; mask = (1L << k); k++)
					if (mask == inpath[j].mask) {
						found = TRUE;
						break;
					} else if (mask & inpath[j].mask)
						break;
			}
			if (!found) {
				showmask(width, inpath[j].mask);
				PRINTF(": %s\n", inpath[j].name);
			}
		}
	}
	(void)exit(0);
	/*NOTREACHED*/
}

failed(s)
char	*s;
{
	perror(s);
	exit(1);
}
