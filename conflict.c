/******************************************************************************
 * Copyright 1988-2024,2025 by Thomas E. Dickey.  All Rights Reserved.        *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/

/*
 * $Id: conflict.c,v 6.22 2025/01/17 23:09:04 tom Exp $
 *
 * Title:	conflict.c (path-conflict display)
 * Author:	T.E.Dickey
 * Created:	15 Apr 1988
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

#include "conflict.h"

#define	CHUNK	0xff		/* (2**n) - 1 chunk for reducing realloc's */

static GCC_NORETURN void usage(void);

static INPATH *inpath;
static DIRS *dirs;
static const char *env_name;	/* e.g., "PATH" */
static char *pathlist;		/* e.g., ".:/bin:/usr/bin" */
static char **FileTypes;
static char *dot;
static size_t total;
static size_t path_len;		/* maximum number of items in path */
static unsigned num_types;	/* number of file-types to scan */
static int acc_mask;		/* permissions we're looking for */
static int caseless;		/* true if we ignore case when comparing names */
static int p_opt;		/* print pathnames (not a table) */
static int v_opt;		/* verbose (repeat for different levels) */
static int do_blips;
static const char *w_opt = "";	/* pads listing */
static const char *w_opt_text = "--------";

#if MIXEDCASE_FILENAMES
#define C_VALUE 0
#define C_USAGE " (default)"
#define I_USAGE ""
#else
#define C_VALUE 1
#define C_USAGE ""
#define I_USAGE " (default)"
#endif

/* just as a stylistic issue, use DOS-compatible names in uppercase */
#if !MIXEDCASE_FILENAMES && (SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX)
#define DOS_upper(s) strupr(s)
#else
#define DOS_upper(s) s
#endif

#if SYS_MSDOS
#define TYPES_PATH ".COM.EXE.BAT.PIF"
#elif SYS_OS2 || SYS_OS2_EMX
#define TYPES_PATH ".EXE.CMD.bat.com.sys"
#elif SYS_WIN32
#define TYPES_PATH ".COM.EXE.BAT.LNK"	/* could also use .CMD and .PIF */
#endif

#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX || SYS_CYGWIN
#define NULL_FTYPE 0		/* "." is the same as "" */
#else
#define NULL_FTYPE 1		/* "." and "" are different types */
#endif

#ifndef HAVE_STRCASECMP
static int
my_strcasecmp(const char *a, const char *b)
{
    while (*a && *b) {
	int cmp = toupper(UCH(*a)) - toupper(UCH(*b));
	if (cmp != 0)
	    break;
	++a;
	++b;
    }
    return toupper(UCH(*a)) - toupper(UCH(*b));
}
#define strcasecmp(a, b) my_strcasecmp(a, b)
static int
my_strncasecmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *b) {
	int cmp = toupper(UCH(*a)) - toupper(UCH(*b));
	if (cmp != 0)
	    break;
	++a;
	++b;
	--n;
    }
    return toupper(UCH(*a)) - toupper(UCH(*b));
}
#define strncasecmp(a, b, n) my_strncasecmp(a, b, n)
#endif

/*
 * comparison procedure used for sorting list of names for display
 */
static int
cmp_INPATH(const void *p1, const void *p2)
{
    return strcmp(((const INPATH *) (p1))->ip_NAME, ((const INPATH *) (p2))->ip_NAME);
}

/*
 * save information from the current stat-block so we can test for repeated
 * instances of the file.
 */
#if USE_INODE
static void
node_found(INPATH * ip, unsigned n, type_t flags, struct stat *sb)
{
    NODEFLAGS(n) |= flags;
    ip->node[n].device = sb->st_dev;
    ip->node[n].inode = sb->st_ino;
}

#define	FoundNode(ip,n) node_found(ip,n,ok,&sb)

#else
static void
node_found(INPATH * ip, unsigned n, type_t flags)
{
    NODEFLAGS(n) |= flags;
}

#define	FoundNode(ip,n) node_found(ip,n,ok)
#endif /* USE_INODE */

/*
 * Look up the given name and fill-in the n'th entry of the 'dirs[]' array.
 */
static int
LookupDirs(char *name, unsigned n)
{
    int found = FALSE;
#if USE_INODE
    struct stat sb;

    if (stat(name, &sb) >= 0) {
	dirs[n].device = sb.st_dev;
	dirs[n].inode = sb.st_ino;
	found = TRUE;
    }
#else
    char resolved[MAXPATHLEN];
    if (realpath(name, resolved) != 0) {
	dirs[n].actual = MakeString(resolved);
	found = TRUE;
    }
#endif
    return found;
}

/*
 * Display the conflicting items
 */
static void
ShowConflicts(unsigned len, INPATH * ip)
{
    unsigned j;
    unsigned k;
    int found = FALSE;
    char flags[BUFSIZ];

    for (j = k = 0; j < len; j++) {
	if (FileTypes == NULL) {
	    register int d = '-';
	    if (ip != NULL) {
		if (IS_A_NODE(j)) {
		    d = '*';
		    if (!found) {
			k = j;	/* latch first index */
			found = TRUE;
		    } else {
			if (!SAME_NODE(j, k))
			    d = '+';
		    }
		}
	    }
	    (void) printf("%s%c", w_opt, d);
	    continue;
	}

	for (k = 0; k < num_types + 2; k++)
	    flags[k] = '-';
	flags[k - 1] = EOS;
	flags[0] = ':';
	if (ip != NULL && IS_A_NODE(j)) {
	    for (k = 0; k < num_types; k++) {
		if ((type_t) (1 << k) & NODEFLAGS(j)) {
		    char *s = FileTypes[k];
		    int c = *s++;
		    if (c == EOS || *s == EOS)
			c = '.';
		    else
			c = *s;
		    flags[k + 1] = (char) c;
		}
	    }
	}
	(void) printf("%s%s", w_opt, flags);
    }
}

static char *
TypesOf(size_t len, INPATH * ip)
{
    unsigned j, mask, n, k;
#if NO_LEAKS
    static char result[BUFSIZ];
#else
    static char *result;

    if (result == NULL)
	result = (char *) malloc((num_types * 4) + 1);
#endif

    for (j = mask = 0; j < len; j++)
	mask |= NODEFLAGS(j);

    result[0] = EOS;
    for (n = 0, k = 1; n < num_types; n++, k <<= 1) {
	if (k & mask) {
	    if (result[0] != EOS)
		(void) strcat(result, ",");
	    (void) strcat(result, FileTypes[n]);
	}
    }
    return result;
}

static void
ShowPathnames(INPATH * ip)
{
    unsigned j, k;
    char bfr[MAXPATHLEN];

    if (num_types != 0) {
	(void) strcpy(bfr, ip->ip_name);
	*ftype(bfr) = EOS;
    }

    if (p_opt) {
	for (j = 0; j < path_len; j++) {
	    if (NODEFLAGS(j) != 0) {
		if (num_types != 0) {
		    for (k = 0; k < num_types; k++) {
			if ((type_t) (1 << k) & NODEFLAGS(j)) {
			    (void) printf("%s%c%s%s\n",
					  dirs[j].name,
					  PATHNAME_SEP,
					  bfr,
					  FileTypes[k]);
			}
		    }
		} else {
		    (void) printf("%s%c%s\n",
				  dirs[j].name,
				  PATHNAME_SEP,
				  ip->ip_name);
		}
	    }
	}
    } else if (num_types != 0) {
	(void) printf(": %s (%s)\n", bfr, TypesOf(path_len, ip));
    } else {
	(void) printf(": %s\n", ip->ip_name);
    }
}

/*
 * Compress the list of paths, removing those which had no conflicts.
 */
static void
compress_list(void)
{
    unsigned j, k, jj;
    int compress;
    type_t flags;

    for (j = 0; j < path_len; j++) {
	for (k = 0, compress = TRUE; compress && (k < total); k++) {
	    register INPATH *ip = inpath + k;
	    if ((flags = NODEFLAGS(j)) != 0) {
		/* If there's more than one suffix found for
		 * the current entry, we cannot compress it.
		 */
		while (flags != 0) {
		    if ((flags & 1) != 0)
			break;
		    flags >>= 1;
		}
		if (flags == 1) {
		    for (jj = 0; jj < path_len; jj++) {
			if (jj == j)
			    continue;
			if (IS_A_NODE(jj)) {
			    compress = FALSE;
			    break;
			}
		    }
		} else {
		    compress = FALSE;
		}
	    }
	}
	if (compress) {
	    if (v_opt) {
		(void) fprintf(stderr, "no conflict:%s\n", dirs[j].name);
		(void) fflush(stderr);
	    }
	    FreeString(dirs[j].name);
	    for (jj = j + 1; jj < path_len; jj++)
		dirs[jj - 1] = dirs[jj];
	    for (k = 0; k < total; k++) {
		register INPATH *ip = inpath + k;
		for (jj = j + 1; jj < path_len; jj++)
		    ip->node[jj - 1] = ip->node[jj];
	    }
	    j--;
	    path_len--;
	}
    }
}

/*
 * returns true iff we have two instances of the same name
 */
static int
had_conflict(INPATH * ip)
{
    unsigned j, k;
    int first = TRUE;
    type_t mask;

    for (j = 0; j < path_len; j++) {
	if ((mask = NODEFLAGS(j)) != 0) {

	    if (FileTypes == NULL) {
		if (!first)
		    return TRUE;
		first = FALSE;
		continue;
	    }

	    for (k = 0; k < num_types; k++) {
		if (mask & (type_t) (1 << k)) {
		    if (!first)
			return TRUE;
		    first = FALSE;
		}
	    }
	}
    }
    return (FALSE);
}

/* Returns nonzero if the given filename has an executable-suffix */
static type_t
LookupType(char *name)
{
    unsigned k;
    char temp[MAXPATHLEN];
    char *type = DOS_upper(strcpy(temp, ftype(name)));

#if !NULL_FTYPE
    if (*type == '\0')
	type = ".";
#endif
    for (k = 0; k < num_types; k++) {
	if (!strcmp(type, FileTypes[k]))
	    return (type_t) (1 << k);
    }
    return 0;
}

static int
SameString(char *a, char *b)
{
    int result;

    if (caseless) {
	result = !strcasecmp(a, b);
    } else {
#if USE_TXTALLOC
	result = (a == b);
#else
	result = !strcmp(a, b);
#endif
    }
    return result;
}

/* Compare two leaf-names, ignoring their suffix. */
static int
SameTypeless(char *a, char *b)
{
    int result = FALSE;
    char *type_a = ftype(a);
    char *type_b = ftype(b);
    if ((type_a - a) == (type_b - b)) {
	if (type_a == a) {
	    result = SameString(a, b);
	} else {
	    if (caseless) {
		if (!strncasecmp(a, b, (size_t) (type_a - a)))
		    result = TRUE;
	    } else {
		if (!strncmp(a, b, (size_t) (type_a - a)))
		    result = TRUE;
	    }
	}
    }
    return result;
}

static int
SameName(char *a, char *b)
{
    int result;
    if (FileTypes == NULL) {
	result = SameString(a, b);
    } else {
	result = SameTypeless(a, b);
    }
    return result;
}

/*
 * For systems where case does not matter, return an uppercased copy of the
 * pathname.
 */
#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX || SYS_CYGWIN
static char *
ToCompare(char *a)
{
    char buffer[MAXPATHLEN];
#if SYS_CYGWIN
    static int strict_case = 0;
    static int first = 1;
    if (first) {
	char *env = getenv("CYGWIN");
	if (env != 0 && strstr(env, "check_case:strict") != 0)
	    strict_case = 1;
	first = 0;
    }
    if (strict_case)
	return a;
#endif
    strncpy(buffer, a, sizeof(buffer))[sizeof(buffer) - 1] = '\0';
    return MakeString(strupr(buffer));
}
#else
#define ToCompare(a) (a)	/* with txtalloc, we still have same pointer */
#endif

static void
ScanConflicts(char *path, unsigned inx, int argc, char **argv)
{
    DIR *dp;
    struct dirent *de;
    struct stat sb;
    int j;
    unsigned k;
#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX
    char save_wd[MAXPATHLEN];
#endif

    /*
     * When scanning a directory, we first chdir to it, mostly to make
     * the scan+stat work faster, but also because some systems don't
     * scan properly otherwise.
     *
     * MSDOS and OS/2 are a little more complicated, because each drive
     * has its own current directory.
     */
#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX
    (void) strcpy(save_wd, dot);
    if (!strcmp(".", path)) {
	path = dot;
    } else if (!same_drive(dot, path)) {
	if (!set_drive(path))
	    return;
	getwd(save_wd);
    }
#endif
    if (v_opt > 2)
	printf("ScanConflicts \"%s\"\n", path);

    if (set_directory(path)
	&& (dp = opendir(path)) != NULL) {

	while ((de = readdir(dp)) != NULL) {
	    register
	    type_t ok = 0;
	    int found = FALSE;
	    char buffer[MAXPATHLEN];
	    char *the_name;
	    char *the_NAME;

	    if (do_blips)
		blip('.');

	    (void) sprintf(buffer, "%.*s", (int) NAMLEN(de), de->d_name);
	    the_name = MakeString(DOS_upper(buffer));
	    the_NAME = ToCompare(the_name);

	    /* If arguments are given, restrict search to them */
	    if (argc > optind) {
		for (j = optind; j < argc; j++) {
		    if (SameName(argv[j], the_name)) {
			found = TRUE;
			break;
		    }
		}
		if (!found)
		    continue;
	    }

	    /* Verify that the name is a file, and executable */
	    if (stat(the_name, &sb) < 0)
		continue;
	    if ((sb.st_mode & S_IFMT) != S_IFREG)
		continue;

#if SYS_UNIX || SYS_OS2 || SYS_OS2_EMX
	    if (access(the_name, acc_mask) < 0)
		continue;
	    ok = 1;
#endif
	    if (FileTypes != NULL) {
		if ((ok = LookupType(the_name)) == 0)
		    continue;
	    }

	    /* Find the name in our array of all names */
	    found = FALSE;
	    for (k = 0; k < total; k++) {
		if (SameName(inpath[k].ip_NAME, the_NAME)) {
		    FoundNode(&inpath[k], inx);
		    found = TRUE;
		    break;
		}
	    }

	    /* If not there, add it */
	    if (found) {
		if (the_NAME != the_name) {
		    FreeString(the_NAME);
		}
	    } else {
		if (!(total & CHUNK)) {
		    size_t need = (((total * 3) / 2) | CHUNK) + 1;
		    if (inpath != NULL)
			inpath = TypeRealloc(INPATH, inpath, need);
		    else
			inpath = TypeAlloc(INPATH, need);
		}
		j = (int) total++;
		inpath[j].ip_name = the_name;
		inpath[j].ip_NAME = the_NAME;
		inpath[j].node = TypeAlloc(NODE, path_len);
		FoundNode(&inpath[j], inx);
	    }
	    if (v_opt > 2) {
		(void) printf("%c %s%c%s\n",
			      found ? '+' : '*',
			      path, PATHNAME_SEP, buffer);
	    }
	}
	(void) closedir(dp);
    }
#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX
    if (strcmp(dot, save_wd)) {
	chdir(save_wd);
    }
#endif
    (void) set_directory(dot);
}

/*
 * Build up the 'pathlist' string in the same form as we expect from an
 * environment variable, from command-line option values.
 */
static void
AddToPath(char *path)
{
    size_t need = strlen(path) + 1;

    if (pathlist != NULL) {
	size_t have = strlen(pathlist);
	pathlist = (char *) realloc(pathlist, have + need + 1);
	(void) sprintf(pathlist + have, "%c%s", PATHLIST_SEP, path);
    } else {
	pathlist = (char *) malloc(need);
	(void) strcpy(pathlist, path);
    }
}

static void
usage(void)
{
    static const char *tbl[] =
    {
	"Usage: conflict [options] [list_of_files]"
	,""
	,"Lists conflicts of executable files in current PATH."
	,"First instance in path (and those linked to it) are marked"
	,"with \"*\", succeeding instances with \"+\" marks."
	,""
	,"Options are:"
	,"  -c         do not ignore case when comparing filenames" C_USAGE
	,"  -e name    specify another environment variable than $PATH"
	,"  -i         ignore case when comparing filenames" I_USAGE
	,"  -p         print pathnames only"
	,"  -r         look for read-able files"
	,"  -t types   look only for given file-types"
	,"  -v         (verbose) show names found in each directory"
	,"             once: shows all directory names"
	,"             twice: shows all filenames"
	,"             thrice: shows trace of directory scanning"
	,"  -V         print version"
	,"  -w         look for write-able files"
	,"  -W number  expand width of display by number of columns"
	,"  -x         look for execut-able files (default)"
	,""
	,"Also, C-preprocessor-style -I and -L options"
    };
    unsigned j;
    for (j = 0; j < SIZEOF(tbl); j++)
	(void) fprintf(stderr, "%s\n", tbl[j]);
    (void) fflush(stderr);
    (void) exit(EXIT_FAILURE);
}

void
failed(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    int found, j, value;
    unsigned k, kk;
    char *s, *t;
    char *type_list = NULL;
    char bfr[MAXPATHLEN];

    caseless = C_VALUE;

    while ((j = getopt(argc, argv, "cD:e:iI:prt:U:vVwW:x")) != EOF) {
	switch (j) {
	case 'V':
	    printf("conflict %s\n", RELEASE);
	    return (EXIT_SUCCESS);

	case 'c':
	    caseless = 0;
	    break;
	case 'i':
	    caseless = 1;
	    break;

	case 'e':
	    env_name = optarg;
	    break;
	case 't':
	    type_list = optarg;
	    break;
	case 'v':
	    v_opt++;
	    break;
	case 'p':
	    p_opt = TRUE;
	    break;

	case 'W':
	    value = (int) strtol(optarg, &t, 0);
	    if (*t != EOS || value < 0)
		usage();
	    value = (int) strlen(w_opt_text) - value;
	    if (value < 0)
		value = 0;
	    w_opt = w_opt_text + value;
	    break;

	case 'r':
	    acc_mask |= R_OK;
	    break;
	case 'w':
	    acc_mask |= W_OK;
	    break;
	case 'x':
	    acc_mask |= X_OK;
	    break;

	    /*
	     * The 'U' and 'D' options are parsed to simplify
	     * using C-preprocessor options to scan for include-
	     * files.
	     */
	case 'U':		/* ignored */
	    break;
	case 'D':		/* ignored */
	    break;
	case 'I':
	    AddToPath(optarg);
	    break;
	case 'L':
	    AddToPath(optarg);
	    break;

	default:
	    usage();
	    /*NOTREACHED */
	}
    }

    /* The "-v" and "-p" options aren't meaningful in combination */
    if (v_opt && p_opt)
	usage();

    /* The remaining arguments are the programs/symbols that we're looking
     * for.  Reduce the argument-list to the leaf-names (stripping paths).
     */
    if (argc > optind) {
	for (j = optind; j < argc; j++) {
	    argv[j] = MakeString(DOS_upper(fleaf(argv[j])));
	}
    }

    do_blips = ((v_opt > 1) && isatty(fileno(stderr)));

    if (acc_mask == 0)
	acc_mask = X_OK;

    /*
     * Get the current working-directory, so we have a reference point to
     * go back after scanning directories.
     */
    if (getwd(bfr) == NULL)
	failed("getcwd");
    dot = MakeString(bfr);
    if (!p_opt)
	(void) printf("Current working directory is \"%s\"\n", dot);

    /*
     * Obtain the list of directories that we'll scan.
     */
    if (pathlist == NULL) {
	if (env_name == NULL)
	    env_name = "PATH";

	if ((s = getenv(env_name)) != NULL)
	    pathlist = strdup(s);
	else
	    failed(env_name);

#if SYS_MSDOS || SYS_OS2 || SYS_WIN32 || SYS_OS2_EMX
	if (!strcmp(env_name, "PATH")) {
	    /* look in current directory before looking in $PATH */
	    s = malloc(strlen(pathlist) + 3);
	    (void) sprintf(s, ".%c%s", PATHLIST_SEP, pathlist);
	    free(pathlist);
	    pathlist = s;
	}
#endif
    }
    for (s = DOS_upper(pathlist), path_len = 1; *s != EOS; s++)
	if (*s == PATHLIST_SEP)
	    path_len++;
    dirs = TypeAlloc(DIRS, path_len);

    /*
     * Reconstruct the type-list (if any) as an array to simplify scanning.
     */
#ifdef TYPES_PATH
    if (type_list == 0) {
	if (!strcmp(env_name, "PATH"))
	    type_list = TYPES_PATH;
    }
#endif
    if (type_list != NULL) {
	type_list = DOS_upper(strdup(type_list));
	for (s = type_list, num_types = 0; *s != EOS; s++) {
	    if (*s == '.') {
		num_types++;
#if NULL_FTYPE			/* "." and "" are different types */
		if ((s[1] == '.') || (s[1] == EOS))
		    num_types++;
#endif
	    }
	}
	if (num_types == 0 || *type_list != '.') {
	    (void) fprintf(stderr, "Type-list must be .-separated\n");
	    exit(EXIT_FAILURE);
	}
	FileTypes = TypeAlloc(char *, num_types);
	j = (int) num_types;
	do {
	    if (*--s == '.') {
		FileTypes[--j] = strdup(s);
#if NULL_FTYPE			/* "." and "" are different types */
		if (s[1] == EOS)
		    FileTypes[--j] = strdup("");
#endif
		*s = EOS;
	    }
	} while (j != 0);
	free(type_list);
    }

    /*
     * Build a list of the directories in the $PATH variable, excluding any
     * that happen to be the same inode (e.g., ".", or symbolically linked
     * directories).
     */
    for (s = pathlist, kk = 0; *s != EOS; s = t) {
	for (k = 0; s[k] != PATHLIST_SEP; k++)
	    if ((bfr[k] = s[k]) == EOS)
		break;

	t = (s[k] != EOS) ? s + k + 1 : s + k;
	if (k == 0)
	    (void) strcpy(bfr, ".");
	else
	    bfr[k] = EOS;

	if (LookupDirs(bfr, kk)) {
	    for (k = 0, found = FALSE; k < kk; k++) {
		if (SAME_DIRS(kk, k)) {
		    found = TRUE;
		    break;
		}
	    }
	    if (!found) {
#if !USE_INODE
		if (strcmp(bfr, "."))
		    dirs[kk].name = MakeString(dirs[kk].actual);
		else
#endif
		    dirs[kk].name = MakeString(bfr);
		kk++;
	    }
	}
    }
    path_len = kk;

    /*
     * For each unique directory in $PATH, scan it, looking for executable
     * files.
     */
    for (k = 0; k < path_len; k++) {
	if (v_opt > 1) {
	    ShowConflicts(k + 1, (INPATH *) 0);
	    (void) printf("> %s\n", dirs[k].name);
	} else if (do_blips) {
	    (void) fprintf(stderr, "%s", dirs[k].name);
	    (void) fflush(stdout);
	    (void) fflush(stderr);
	}
	ScanConflicts(dirs[k].name, k, argc, argv);
	if (do_blips)
	    blip('\n');
    }

    /*
     * The normal (unverbose) listing shows only the conflicting files,
     * and the directories in which they are found.
     */
    if (!v_opt)
	compress_list();

    if (!p_opt) {
	for (k = 0; k < path_len; k++) {
	    ShowConflicts(k + 1, (INPATH *) 0);
	    (void) printf("> %s\n", dirs[k].name);
	}
    }

    if (total != 0) {
	qsort((char *) inpath, (size_t) total, sizeof(INPATH), cmp_INPATH);
	for (k = 0; k < total; k++) {
	    if ((v_opt > 1) || had_conflict(&inpath[k])) {
		if (!p_opt)
		    ShowConflicts((unsigned) path_len, &inpath[k]);
		ShowPathnames(&inpath[k]);
	    }
	}
    }

    /*
     * Test/debug: free all memory so we can validate the heap.
     */
#if NO_LEAKS
    if (FileTypes != 0) {
	for (k = 0; k < num_types; k++)
	    free(FileTypes[k]);
	free((char *) FileTypes);
    }
    if (dirs != 0) {
# if !USE_TXTALLOC
	for (k = 0; k < path_len; k++) {
	    FreeString(dirs[k].name);
	}
# endif
	free((char *) dirs);
    }
    if (inpath != 0) {
	for (k = 0; k < total; k++) {
	    if (inpath[k].ip_NAME != inpath[k].ip_name) {
		FreeString(inpath[k].ip_NAME);
	    }
	    FreeString(inpath[k].ip_name);
	    free((char *) (inpath[k].node));
	}
	free((char *) inpath);
    }
    free(pathlist);
    FreeString(dot);
# if USE_TXTALLOC
    free_txtalloc();
# endif
# if HAVE_DBMALLOC_H
    malloc_dump(fileno(stdout));
# endif
#endif
    (void) fflush(stderr);
    (void) fflush(stdout);
    return EXIT_SUCCESS;
}
