#!/bin/sh
# $Id: run_test.sh,v 6.0 1995/03/18 13:33:43 dickey Rel $
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
cat <<eof/
**
**	Add a dummy executable in the temp-directory, producing a conflict:
eof/
cd /tmp
rm -f conflict
echo test >conflict
chmod 755 conflict
$PROG
rm -f conflict
