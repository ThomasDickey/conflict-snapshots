#!/bin/sh
###############################################################################
# Copyright 1989-2004,2020 by Thomas E. Dickey.  All Rights Reserved.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of the above listed copyright holder(s)
# not be used in advertising or publicity pertaining to distribution of the
# software without specific, written prior permission.
#
# THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
# TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE
# FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
# IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
################################################################################
# $Id: run_test.sh,v 6.5 2020/10/11 17:32:05 tom Exp $
# Run a test to show that CONFLICT is working
BIN=`pwd`
PROG=$BIN/conflict
PATH=".:$BIN:/bin"; export PATH
#
cat <<eof/
**
**	Set PATH = $PATH
**	This produces no conflict (unless /bin contains conflict!):
eof/
$PROG
#
cat <<eof/
**
**	Set PATH = $PATH
**	This shows conflict between different filetypes
eof/
$PROG -r -t.c.o. conflict
cat <<eof/
**
**	Set PATH = $PATH
**	This repeats the last test, with pathnames-only
eof/
$PROG -p -r -t.c.o. conflict

# make a temporary directory
DIR=`mktemp -d ${TMPDIR-/tmp}/conflict.XXXXXX 2>/dev/null`
if test -z "$DIR" ; then
	DIR=/tmp/conflict.$$
	mkdir $DIR
fi
trap "cd /;rm -rf $DIR" EXIT INT QUIT HUP TERM
cd $DIR || exit 1

cat <<eof/
**
**	Add a dummy executable in the current directory
**		$DIR
**
**	producing a conflict with the executable
**		$PROG
**
**	For Unix, the conflict will be in the empty suffix (first column
**	of the report).  For Win32 it will be in the combination of empty
**	and "e" (.exe) suffixes.
eof/

# try to test conflicts on case-insensitive systems
TEST=conflict
rm -f $TEST
cat >$TEST <<EOF
EOF
#touch $TEST
if test -f Conflict ; then
	TEST=Conflict
	rm -f $TEST
fi

echo "#!${SHELL-/bin/sh}" >$TEST
echo echo hello >>$TEST
chmod 755 $TEST
$PROG -t..exe
