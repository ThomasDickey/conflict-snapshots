#ifndef	lint
static	char	Id[] = "$Header: /users/source/archives/conflict.vcs/RCS/conflict.c,v 3.1 1991/10/22 16:53:11 dickey Exp $";
#endif

/*
 * Title:	conflict.c (path-conflict display)
 * Author:	T.E.Dickey
 * Created:	15 Apr 1988
 * Modified:
 *		22 Oct 1991, converted to ANSI
 *		21 May 1991, added "-a" option, making default more compact by
 *			     suppressing directories which contain no conflicts
 *		07 Dec 1989, recoded so that we aren't constrained by the number
 *			     of bits in a long to record information.  A side-
 *			     effect of this is that we can show instances of
 *			     files linked (with the same name) to the first
 *			     instance found.
 *		07 Dec 1989, added "-w" option, enhanced usage-message.
 *			     Corrected bug which caused wrong behavior when
 *			     options were given w/o names of files-to-find.
 *		25 Jul 1989, moved the code which lists the pathnames down past
 *			     the point at which we have eliminated redundant
 *			     paths, to avoid a confusing display.
 *		
 *		25 Jan 1989, corrected array-index variable
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
 */

#define		DIR_PTYPES	/* include directory-definitions */
#define		STR_PTYPES
#include	<ptypes.h>

#define	CHUNK	0xff		/* (2**n) - 1 chunk for reducing realloc's */
#define	IS_A_NODE(j)		(ip->node[j].device != 0\
			||	 ip->node[j].inode  != 0)
#define	SAME_NODE(j,k)		(ip->node[j].device == ip->node[k].device\
			&&	 ip->node[j].inode  == ip->node[k].inode)

typedef	struct	{
		dev_t	device;
		ino_t	inode;
	} NODE;

typedef	struct	{
		char	*name;	/* name of executable file */
		NODE	*node;	/* stat-result, for comparing */
	} INPATH;

#define	def_doalloc	INPATH_alloc
	/*ARGSUSED*/
	def_DOALLOC(INPATH)

static	INPATH	*inpath;
static	char	*pathlist;
static	char	dot[BUFSIZ];
static	unsigned total,
		path_len;	/* maximum number of items in path */
static	int	a_opt,		/* shows all pathnames, even if no conflict */
		l_opt,		/* long report: shows all items */
		v_opt,		/* verbose */
		my_uid, my_gid, root;
static	char	*w_opt	= "",	/* pads listing */
		*w_opt_text = "--------";

/*
 * comparison procedure used for sorting list of names for display
 */
static
compar(
_ARX(INPATH *,	p1)
_AR1(INPATH *,	p2)
	)
_DCL(INPATH *,	p1)
_DCL(INPATH *,	p2)
{
	return (strcmp(p1->name, p2->name));
}

/*
 * allocate a vector of stat-blocks so we can test for repeats/links
 */
static
NODE	*
node_alloc(_AR0)
{
	int	size	= sizeof(NODE) * path_len;
	char	*vec	= doalloc((char *)0, (unsigned)size);
	bzero(vec, size);	/* zeroed device & inode don't exist! */
#ifdef	lint
	return ((NODE *)0);
#else
	return ((NODE *)vec);
#endif
}

/*
 * save information from the current stat-block so we can test for repeated
 * instances of the file.
 */
static
node_found(
_ARX(INPATH *,	ip)
_ARX(int,	inx)
_AR1(struct stat *,sb)
	)
_DCL(INPATH *,	ip)
_DCL(int,	inx)
_DCL(struct stat *,sb)
{
	ip->node[inx].device = sb->st_dev;
	ip->node[inx].inode  = sb->st_ino;
}

/*
 * Display the conflicting items
 */
static
show_conflict(
_ARX(int,	len)
_AR1(INPATH *,	ip)
	)
_DCL(int,	len)
_DCL(INPATH *,	ip)
{
	register int j, d;
	auto	int	k = -1;

	for (j = 0; j < len; j++) {
		d = '-';
		if (ip != 0) {
			if (IS_A_NODE(j)) {
				d = '*';
				if (k >= 0) {
					if (!SAME_NODE(j,k))
						d = '+';
				} else
					k = j;	/* latch first index */
			}
		}
		PRINTF("%s%c", w_opt, d);
	}
}

/*
 * compresses the list of paths, removing those which had no conflicts.
 */
static
compress_list(
_AR1(char **,	vec))
_DCL(char **,	vec)
{
	register int	j, k, jj;
	int	compress;

	for (j = 0; j < path_len; j++) {
		compress = TRUE;
		for (k = 0; compress && (k < total); k++) {
			register INPATH *ip = inpath + k;
			if (IS_A_NODE(j)) {
				for (jj = 0; jj < path_len; jj++) {
					if (jj == j)
						continue;
					if (IS_A_NODE(jj)) {
						compress = FALSE;
						break;
					}
				}
			}
		}
		if (compress) {
			if (v_opt)
				FPRINTF(stderr, "no conflict:%s\n", vec[j]);
			for (jj = j+1; jj < path_len; jj++)
				vec[jj-1] = vec[jj];
			for (k = 0; k < total; k++) {
				register INPATH *ip = inpath + k;
				for (jj = j+1; jj < path_len; jj++)
					ip->node[jj-1] = ip->node[jj];
			}
			j--;
			path_len--;
		}
	}
}

/*
 * returns true iff we have two instances of the same name
 */
static
had_conflict(
_AR1(INPATH *,	ip))
_DCL(INPATH *,	ip)
{
	register int	j;
	auto	int	first	= TRUE;

	for (j = 0; j < path_len; j++) {
		if (IS_A_NODE(j)) {
			if (first)
				first = FALSE;
			else
				return (TRUE);
		}
	}
	return (FALSE);
}

static
conflict(
_ARX(char *,	path)
_ARX(int,	inx)
_ARX(int,	argc)
_AR1(char **,	argv)
	)
_DCL(char *,	path)
_DCL(int,	inx)
_DCL(int,	argc)
_DCL(char **,	argv)
{
	auto	DIR		*dp;
	auto	struct	direct	*de;
	auto	struct	stat	sb;
	register int		j;

	if (dp = opendir(path)) {
		(void)chdir(path);

		while (de = readdir(dp)) {
		register
		int	ok	= FALSE,
			found	= FALSE;
		char	*s;

			if (!a_opt && v_opt)
				blip('.');

			/* If arguments are given, restrict search to them */
			if (argc > optind) {
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
					node_found(&inpath[j], inx, &sb);
					found = TRUE;
					break;
				}
			}

			/* If not there, add it */
			if (!found) {
				if (!(total & CHUNK)) {
					unsigned need = (total | CHUNK) + 1;
					inpath = DOALLOC(inpath, INPATH, need);
				}
				j = total++;
				inpath[j].name = stralloc(de->d_name);
				inpath[j].node = node_alloc();
				node_found(&inpath[j], inx, &sb);
			}
			if (v_opt > 1)
				PRINTF("%c %s/%s\n",
					found ? '+' : '*',
					path, de->d_name);
		}
		closedir(dp);
		(void)chdir(dot);
	}
}

static
usage(_AR0)
{
	static	char	*tbl[] = {
		 "Usage: conflict [options] [list_of_files]"
		,""
		,"Lists conflicts of executable files in current PATH."
		,"First instance in path (and those linked to it) are marked"
		,"with \"*\", succeeding instances with \"+\" marks."
		,""
		,"Options are:"
		,"  -a         show all paths from $PATH"
		,"  -l         list all executable files (even w/o conflict)"
		,"  -v         (verbose) show names found in each directory"
		,"  -w number  expand width of display by number of columns"
	};
	register int	j;
	for (j = 0; j < sizeof(tbl)/sizeof(tbl[0]); j++)
		FPRINTF(stderr, "%s\n", tbl[j]);
	(void)fflush(stderr);
	(void)exit(FAIL);
}

/*ARGSUSED*/
_MAIN
{
register
int	found, j, k;
char	*s, *t,
	**absolute,
	**relative,
	full_path[BUFSIZ],
	bfr[BUFSIZ];

	while ((j = getopt(argc, argv, "?alvw:")) != EOF) switch (j) {
	case 'a':	a_opt = TRUE;	break;
	case 'l':	l_opt = TRUE;	break;
	case 'v':	v_opt++;	break;
	case 'w':	k = strtol(optarg, &t, 0);
			if (*t != EOS || k < 0)
				usage();
			k = strlen(w_opt_text) - k;
			if (k < 0)
				k = 0;
			w_opt = w_opt_text + k;
			break;
	default:	usage();
			/*NOTREACHED*/
	}

	if (getwd(dot) == 0)
		failed("getcwd");
	PRINTF("Current working directory is \"%s\"\n", dot);

	pathlist = stralloc(getenv("PATH"));
	for (s = pathlist, j = 0; *s; s++)
		if (*s == ':')
			j++;
	absolute = vecalloc(path_len = ++j);
	relative = vecalloc(path_len);

	my_uid = getuid();
	my_gid = getgid();
	root   = (my_uid == 0);

	for (s = pathlist, j = 0; *s; s++) {
		for (t = s; *t; t++)
			if (*t == ':')
				break;
		if (s == t)
			continue;
		strncpy(bfr, s, (size_t)(t-s))[t-s] = EOS;
		abspath(strcpy(full_path, bfr));
		s = t;
		found = FALSE;
		for (k = 0; k < j; k++) {
			if (!strcmp(full_path, absolute[k])) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			absolute[j] = stralloc(full_path);
			relative[j] = stralloc(bfr);
			if (a_opt) {
				show_conflict(j+1, (INPATH *)0);
				PRINTF("> %s\n", bfr);
			} else if (v_opt) {
				FPRINTF(stderr, "%s", bfr);
				(void) fflush (stdout);
				(void) fflush (stderr);
			}
			conflict(absolute[j], j, argc, argv);
			if (!a_opt && v_opt)
				blip('\n');
			j++;
		}
	}
	path_len = j;		/* reduce to non-redundant paths */

	if (!a_opt) {
		compress_list(relative);
		for (j = 0; j < path_len; j++) {
			show_conflict(j+1, (INPATH *)0);
			PRINTF("> %s\n", relative[j]);
		}
	}

	if (total != 0) {
		qsort((char *)inpath, (LEN_QSORT)total, sizeof(INPATH), compar);
		for (j = 0; j < total; j++) {
			if (l_opt || had_conflict(&inpath[j])) {
				show_conflict((int)path_len, &inpath[j]);
				PRINTF(": %s\n", inpath[j].name);
			}
		}
	}
	(void)exit(SUCCESS);
	/*NOTREACHED*/
}
