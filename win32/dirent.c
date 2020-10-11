/******************************************************************************
 * Copyright 1999,2020 by Thomas E. Dickey.  All Rights Reserved.             *
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

#include <windows.h>
#include <stdio.h>

#include <dirent.h>

#define is_slashc(c) ((c) == '/' || (c) == '\\')
#define WILDCARD "*"

DIR *
opendir(const char *fname)
{
    char buf[MAX_PATH];
    DIR *od;

    (void) strcpy(buf, fname);

    if (!strcmp(buf, "."))	/* if its just a '.', replace with '*.*' */
	(void) strcpy(buf, WILDCARD);
    else {
	/* If the name ends with a slash, append '*.*' otherwise '\*.*' */
	if (!is_slashc(buf[strlen(buf) - 1]))
	    (void) strcat(buf, "\\");
	(void) strcat(buf, WILDCARD);
    }

    /* allocate the structure to maintain currency */
    if ((od = (DIR *) malloc(sizeof(DIR))) == 0)
	return NULL;

    /* Let's try to find a file matching the given name */
    if ((od->hFindFile = FindFirstFile(buf, &od->ffd))
	== INVALID_HANDLE_VALUE) {
	free(od);
	return NULL;
    }
    od->first = 1;
    return od;
}

DIRENT *
readdir(DIR * dirp)
{
    if (dirp->first)
	dirp->first = 0;
    else if (!FindNextFile(dirp->hFindFile, &dirp->ffd))
	return NULL;
    dirp->de.d_name = dirp->ffd.cFileName;
    return &dirp->de;
}

int
closedir(DIR * dirp)
{
    FindClose(dirp->hFindFile);
    free(dirp);
    return 0;
}
