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

#ifndef DIRENT_H
#define DIRENT_H

#include <direct.h>

struct dirent {
    char *d_name;
};

struct _dirdesc {
    HANDLE hFindFile;
    WIN32_FIND_DATA ffd;
    int first;
    struct dirent de;
};

#define DIRENT struct dirent
typedef struct _dirdesc DIR;

#define opendir  my_opendir
#define readdir  my_readdir
#define closedir my_closedir

extern DIR *opendir(const char *path);
extern DIRENT *readdir(DIR * dp);
extern int closedir(DIR * dp);

#endif /* dirent.h */
