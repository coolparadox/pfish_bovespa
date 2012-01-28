#!/bin/sh

set -e

usage() {
	echo "usage: $(basename $0) <COTAHIST_DIR>" 1>&2
	exit 1
}

COTAHIST_DIR="$1"
test -n "$COTAHIST_DIR" || usage
shift
test $# -eq 0 || usage

fail_with() {
	echo "$(basename $0): error: $*" 1>&2
	exit 1
}

test -d "$COTAHIST_DIR" || fail_with "cannot access directory '${COTAHIST_DIR}'."

ask() {
	local PROMPT="$1"
	local ANSWER
	read -n 1 -p "$1" ANSWER
	echo 1>&2
	test "$ANSWER" = 'y' -o "$ANSWER" = 'Y' && return 0
	return 1
}

ZIP_NAMES_FILE=$(mktemp)
find "$COTAHIST_DIR" -mindepth 1 -maxdepth 1 -type f -regextype posix-extended -regex '.*/COTAHIST_.*\.ZIP' -exec basename '{}' ';' 1>$ZIP_NAMES_FILE
test -s $ZIP_NAMES_FILE && { ask "unzip and delete: $( tr '\n' ' ' 0<$ZIP_NAMES_FILE )? " || { rm -f $ZIP_NAMES_FILE ; exit 1 ; } ; }
while read ZIP_NAME ; do
	ZIP_FILE="${COTAHIST_DIR}/${ZIP_NAME}"
	COTAHIST_NAME=$( echo $ZIP_NAME | sed -e 's/\.[^\.]*$//' )
	COTAHIST_FILE="${COTAHIST_DIR}/${COTAHIST_NAME}"
	(
		set -x
		unzip -p $ZIP_FILE 1>$COTAHIST_FILE
		rm -f $ZIP_FILE
	)
done 0<$ZIP_NAMES_FILE
rm -f $ZIP_NAMES_FILE

ask "erase pfish_bovespa stock database? " || exit 1
pfish_bovespa_database_init -f

import() {
	local COTAHIST_FILE="$1"
	(
		set -x
		pfish_bovespa_file_import 0<$COTAHIST_FILE
	)
}

YEARS_FILE=$(mktemp)
find "$COTAHIST_DIR" -mindepth 1 -maxdepth 1 -type f -name 'COTAHIST_A????' | \
sed -r -e 's|.*_A||' | \
sort -n 1>$YEARS_FILE
test -s $YEARS_FILE && { ask "import cotahist files for years: $( tr '\n' ' ' 0<$YEARS_FILE )? " || { rm -r $YEARS_FILE ; exit 1 ; } ; }
while read YEAR ; do
	COTAHIST_NAME="COTAHIST_A${YEAR}"
	import "${COTAHIST_DIR}/${COTAHIST_NAME}"
done 0<$YEARS_FILE
rm -f $YEARS_FILE

MONTHS_FILE=$(mktemp)
find "$COTAHIST_DIR" -mindepth 1 -maxdepth 1 -type f -name 'COTAHIST_M??????' | \
sed -r -e 's|.*_M||' -e 's/(..)(.*)/\2-\1/' | \
sort -t '-' -n -k 1 -n -k 2 1>$MONTHS_FILE
test -s $MONTHS_FILE && { ask "import cotahist files for months: $( tr '\n' ' ' 0<$MONTHS_FILE )? " || { rm -r $MONTHS_FILE ; exit 1 ; } ; }
while IFS='-' read YEAR MONTH ; do
	COTAHIST_NAME="COTAHIST_M${MONTH}${YEAR}"
	import "${COTAHIST_DIR}/${COTAHIST_NAME}"
done 0<$MONTHS_FILE
rm -f $MONTHS_FILE

DAYS_FILE=$(mktemp)
find "$COTAHIST_DIR" -mindepth 1 -maxdepth 1 -type f -name 'COTAHIST_D????????' | \
sed -r -e 's|.*_D||' -e 's/(..)(..)(.*)/\3-\2-\1/' | \
sort -t '-' -n -k 1 -n -k 2 -n -k 3 1>$DAYS_FILE
test -s $DAYS_FILE && { ask "import cotahist files for days: $( tr '\n' ' ' 0<$DAYS_FILE )? " || { rm -r $DAYS_FILE ; exit 1 ; } ; }
while IFS='-' read YEAR MONTH DAY ; do
	COTAHIST_NAME="COTAHIST_D${DAY}${MONTH}${YEAR}"
	import "${COTAHIST_DIR}/${COTAHIST_NAME}"
done 0<$DAYS_FILE
rm -f $DAYS_FILE

BDIS_FILE=$(mktemp)
find "$COTAHIST_DIR" -mindepth 1 -maxdepth 1 -type f -name 'bdi_06_????????.pdf' | \
sed -r -e 's/.*_//' -e 's/\..*//' -e 's/(.{4})(..)(.*)/\1-\2-\3/' | \
sort -t '-' -n -k 1 -n -k 2 -n -k 3 1>$BDIS_FILE
test -s $BDIS_FILE && { ask "import bdi files for days: $( tr '\n' ' ' 0<$BDIS_FILE )? " || { rm -r $BDIS_FILE ; exit 1 ; } ; }
while IFS='-' read YEAR MONTH DAY ; do
	BDI_NAME="bdi_06_${YEAR}${MONTH}${DAY}"
	BDI_FILE="${COTAHIST_DIR}/${BDI_NAME}.pdf"
	(
		set -x
		pfish_bovespa_bdi_06_pdftocotahist "$BDI_FILE" | \
		pfish_bovespa_file_import
	)
done 0<$BDIS_FILE
rm -f $BDIS_FILE


