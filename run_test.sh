#!/bin/sh
# $Id: run_test.sh,v 5.0 1991/10/22 17:05:06 ste_cm Rel $
# Run a test to show that CONFLICT is working
BIN=`cd ../bin; pwd`
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
**	Add a dummy executable in the current directory, producing a conflict:
eof/
rm -f conflict
echo test >conflict
chmod 755 conflict
$PROG
rm -f conflict
#
cat <<eof/
**
**	Change directories to
**		$BIN
**	Show conflict (should be none, since CONFLICT eliminates the
**	redundancy with ".":
eof/
cd ../bin
$PROG
