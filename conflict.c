#ifndef	NO_IDENT
static	char	Id[] = "$Header: /users/source/archives/conflict.vcs/RCS/conflict.c,v 5.2 1993/11/30 19:15:18 dickey Exp $";
#endif

/*
 * Title:	conflict.c (path-conflict display)
 * Author:	T.E.Dickey
 * Created:	15 Apr 1988
 * Modified:
 *		28 Nov 1993, adaptation to MS-DOS.
 *		22 Sep 1993, gcc-warnings, memory leaks.
 *		17 Jul 1992, corrected error parsing pathlist.
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

#define	QSORT_SRC	INPATH
#include	<td_qsort.h>

#define	CHUNK	0xff		/* (2**n) - 1 chunk for reducing realloc's */

#ifdef	unix
#define	IS_A_NODE(j)		(ip->node[j].device != 0\
			||	 ip->node[j].inode  != 0)
#define	SAME_NODE(j,k)		(ip->node[j].device == ip->node[k].device\
			&&	 ip->node[j].inode  == ip->node[k].inode)

typedef	struct	{
		dev_t	device;
		ino_t	inode;
	} NODE;
#endif	/* unix */

#ifdef	MSDOS
#define	IS_A_NODE(j)		(ip->node[j].flags != 0)
#define	SAME_NODE(j,k)		TRUE	/* always on the same machine? */
typedef	struct	{
		int	flags;
	} NODE;
#endif

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
		v_opt;		/* verbose */
static	char	*w_opt	= "",	/* pads listing */
		*w_opt_text = "--------";

#ifdef unix
static	int	my_uid, my_gid;
#define	im_root (my_uid == 0)
#endif

#ifdef	MSDOS
/* executable file-types, ordered by precedence */
static	char	*exe_types[] = {
		"PIF",
		"BAT",
		"EXE",
		"COM"
		};
#endif
/*
 * comparison procedure used for sorting list of names for display
 */
static
QSORT_FUNC(compar)
{
	QSORT_CAST(q1,p1)
	QSORT_CAST(q2,p2)
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
	memset(vec, 0, size);	/* zeroed device & inode don't exist! */
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
#ifdef	unix
static
void	node_found(
	_ARX(INPATH *,	ip)
	_ARX(int,	inx)
	_AR1(STAT *,	sb)
		)
	_DCL(INPATH *,	ip)
	_DCL(int,	inx)
	_DCL(STAT *,	sb)
{
	ip->node[inx].device = sb->st_dev;
	ip->node[inx].inode  = sb->st_ino;
}
#define	FoundNode(ip,inx) node_found(ip,inx,&sb)
#endif	/* unix */

#ifdef	MSDOS
static
void	node_found(INPATH *ip, int inx, int flags)
{
	ip->node[inx].flags |= flags;
}
#define	FoundNode(ip,inx) node_found(ip,inx,ok)
#endif	/* MSDOS */

/*
 * Display the conflicting items
 */
static
void	show_conflict(
	_ARX(int,	len)
	_AR1(INPATH *,	ip)
		)
	_DCL(int,	len)
	_DCL(INPATH *,	ip)
{
	register int j;
	auto	int	k = -1;

	for (j = 0; j < len; j++) {
#ifdef	unix
		register int d = '-';
		if (ip != 0) {
			if (IS_A_NODE(j)) {
				d = '*';
				if (k < 0) {
					k = j;	/* latch first index */
				} else {
					if (!SAME_NODE(j,k))
						d = '+';
				}
			}
		}
		PRINTF("%s%c", w_opt, d);
#endif
#ifdef	MSDOS
		char flags[5];
		(void)strcpy(flags, "----");
		if (ip != 0 && IS_A_NODE(j)) {
			for (k = 0; k < SIZEOF(exe_types); k++) {
				if ((1 << k) & ip->node[j].flags)
					flags[k] = *exe_types[k];
			}
		}
		PRINTF("%s%s", w_opt, flags);
#endif
	}
}

/*
 * compresses the list of paths, removing those which had no conflicts.
 */
static
void	compress_list(
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
			free(vec[j]);
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
int	had_conflict(
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

#ifdef	MSDOS
/* Returns nonzero if the given filename has an executable-suffix */
static
int	ExecutableType(
	_AR1(char *,	name))
	_DCL(char *,	name)
{
	register int k;
	char	*type = ftype(name);
	char	temp[20];
	type = strucpy(temp, type);
	if (*type++ == '.') {
		for (k = 0; k < SIZEOF(exe_types); k++) {
			if (!strcmp(type, exe_types[k]))
				return (1<<k);
		}
	}
	return 0;
}

/* Compare two leaf-names, ignoring their suffix. */
static
int	SameName(
	_ARX(char *,	a)
	_AR1(char *,	b)
		)
	_DCL(char *,	a)
	_DCL(char *,	b)
{
	char	*type_a = ftype(a);
	char	*type_b = ftype(b);
	if (type_a - a == type_b - b) {
		if (!strncmp(a, b, (size_t)(type_a - a)))
			return TRUE;
	}
	return FALSE;
}
#else	/* unix */
#define SameName(a,b) !strcmp(a,b)
#endif

static
void	conflict(
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
	auto	DIR	*dp;
	auto	DIRENT	*de;
	auto	STAT	sb;
	register int		j;

	if ((dp = opendir(path)) != NULL) {
		(void)chdir(path);

		while ((de = readdir(dp)) != NULL) {
		register
		int	ok	= FALSE,
			found	= FALSE;
		char	*s;

			if (!a_opt && v_opt)
				blip('.');

			/* If arguments are given, restrict search to them */
			if (argc > optind) {
				for (j = optind; j < argc; j++) {
					if ((s = fleaf(argv[j])) == NULL)
						s = argv[j];
					if (SameName(s, de->d_name)) {
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

#ifdef	unix
#define	isEXEC(n)	(sb.st_mode & (S_IEXEC >> n))

			if (isEXEC(6))		/* world-executable */
				ok = TRUE;
			else if ((im_root || (my_gid == sb.st_gid))
			&&	isEXEC(3))	/* group-executable */
				ok = TRUE;
			else if ((im_root || (my_uid == sb.st_uid))
			&&	isEXEC(0))	/* owner-executable */
				ok = TRUE;
#endif
#ifdef	MSDOS
			ok = ExecutableType(de->d_name);
#endif
			if (!ok)
				continue;

			/* Find the name in our array of all names */
			found	= FALSE;
			for (j = 0; j < total; j++) {
				if (SameName(inpath[j].name, de->d_name)) {
					FoundNode(&inpath[j], inx);
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
				FoundNode(&inpath[j], inx);
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
void	usage(_AR0)
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
	for (j = 0; j < SIZEOF(tbl); j++)
		FPRINTF(stderr, "%s\n", tbl[j]);
	FFLUSH(stderr);
	(void)exit(FAIL);
}

/*ARGSUSED*/
_MAIN
{
#if	NO_LEAKS
	unsigned free_len;
#endif
	register int	found, j, k;
	char	*s, *t,
		**absolute,
		**relative,
		full_path[BUFSIZ],
		bfr[BUFSIZ];

	while ((j = getopt(argc, argv, "?alvw:")) != EOF) switch (j) {
	case 'a':	a_opt = TRUE;	break;
	case 'l':	l_opt = TRUE;	break;
	case 'v':	v_opt++;	break;
	case 'w':	k = (int)strtol(optarg, &t, 0);
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
#ifdef	MSDOS
	/* look in current directory before looking in $PATH */
	s = doalloc((char *)0, strlen(pathlist)+3);
	FORMAT(s, ".%c%s", PATHLIST_SEP, pathlist);
	free(pathlist);
	pathlist = s;
#endif
	for (s = pathlist, j = 0; *s != EOS; s++)
		if (*s == PATHLIST_SEP)
			j++;
	absolute = vecalloc(path_len = ++j);
	relative = vecalloc(path_len);

#ifdef	unix
	my_uid  = getuid();
	my_gid  = getgid();
#endif

	for (s = pathlist, j = 0; *s;) {

		for (k = 0; s[k] != EOS; k++)
			if (s[k] == PATHLIST_SEP)
				break;
		t = s + k;
		if (k == 0) {	/* should have null-field only at beginning */
			s++;
			continue;
		}
		strncpy(bfr, s, (size_t)(k))[k] = EOS;
		abspath(strcpy(full_path, bfr));
		s = *t ? t+1 : t;

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
				FFLUSH(stdout);
				FFLUSH(stderr);
			}
			conflict(absolute[j], j, argc, argv);
			if (!a_opt && v_opt)
				blip('\n');
			j++;
		}
	}
#if	NO_LEAKS
	free_len = path_len = j;	/* reduce to non-redundant paths */
#endif

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
#if NO_LEAKS
	for (j = 0; j < free_len; j++) {
		free(absolute[j]);
	}
	for (j = 0; j < path_len; j++) {
		free(relative[j]);
	}
	vecfree(absolute);
	vecfree(relative);
#endif
	(void)exit(SUCCESS);
	/*NOTREACHED*/
}
