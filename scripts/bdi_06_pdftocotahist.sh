#!/bin/sh
#

set -e

usage() {
	echo 'usage: pfish_bovespa_bdi_06_pdftocotahist bdi_06_AAAAMMDD.pdf' 1>&2
	exit 1
}

BDI_PDF_FILE="$1"
test -n "$BDI_PDF_FILE" || usage
shift
test $# -eq 0 || usage

DATE=$( basename "$BDI_PDF_FILE" | sed -r -e 's/^bdi_06_//' -e 's/\..*//' )
YEAR=$( echo $DATE | sed -r -e 's/^bdi_06_//' -e 's/\..*//' -e 's/.{4}$//' )
MONTH=$( echo $DATE | sed -r -e 's/^bdi_06_//' -e 's/\..*//' -e 's/.{2}$//' -e 's/^.{4}//' )
DAY=$( echo $DATE | sed -r -e 's/^bdi_06_//' -e 's/\..*//' -e 's/^.{6}//' )

repeat_chars() {
	local HOW_MANY_CHARS=$1
	local CHARACTER="$2"
	for I in $(seq 1 ${HOW_MANY_CHARS}) ; do
		echo -n "$CHARACTER"
	done
}

whites() {
	local HOW_MANY_CHARS=$1
	repeat_chars $HOW_MANY_CHARS ' '
}

echo -n "00COTAHIST.AAAABOVESPA ${YEAR}${MONTH}${DAY}"
whites 214
echo

BDI_TXT_FILE=$(mktemp)
pfish_bovespa_bdi_06_pdftotext "$BDI_PDF_FILE" 1>$BDI_TXT_FILE
HOW_MANY_LINES=$( wc -l 0<$BDI_TXT_FILE )

zeros() {
	local HOW_MANY_CHARS=$1
	repeat_chars $HOW_MANY_CHARS '0'
}

pad_number() {
	local NUMBER=$1
	local TOTAL_DIGITS=$2
	test "$NUMBER" = '-' && NUMBER='0'
	NUMBER=$(echo $NUMBER | sed -e 's/\.//g')
	local HOW_MANY_DIGITS_IN_NUMBER=$( echo -n "$NUMBER" | wc -m )
	local HOW_MANY_DIGITS_LEFT=$((TOTAL_DIGITS-HOW_MANY_DIGITS_IN_NUMBER))
	zeros $HOW_MANY_DIGITS_LEFT
	echo -n $NUMBER
}

pad_text() {
	local TEXT="$1"
	local TOTAL_CHARS=$2
	local HOW_MANY_CHARS_IN_TEXT=$( echo -n "$TEXT" | wc -m )
	local HOW_MANY_CHARS_LEFT=$((TOTAL_CHARS-HOW_MANY_CHARS_IN_TEXT))
	echo -n $TEXT
	whites $HOW_MANY_CHARS_LEFT
}

pad_price() {
	local PRICE="$1"
	local TOTAL_CHARS=$2
	pad_number "$(echo $PRICE | sed -e 's/,//')" $TOTAL_CHARS
}

while read CODIGO NOME ABERT MIN MAX MED FECH OSC COMPRA VENDA NUM_NEG QUANTIDADE ; do
	echo -n '01'
	echo -n "${YEAR}${MONTH}${DAY}"
	echo -n '02'
	pad_text "$CODIGO" 12
	echo -n '010'
	pad_text "$(echo $NOME | sed -e 's/_/ /g')" 22
	echo -n '   '
	pad_text 'R$' 4
	pad_price $ABERT 13
	pad_price $MAX 13
	pad_price $MIN 13
	pad_price $MED 13
	pad_price $FECH 13
	pad_price $COMPRA 13
	pad_price $VENDA 13
	pad_number $NUM_NEG 5
	pad_number $QUANTIDADE 18
	if test "$MED" = '-' -o "$QUANTIDADE" = '-' ; then
		pad_number 0 18
	else
		_MED=$(echo $MED | sed -e 's/[,\.]//g')
		_QUANTIDADE=$(echo $QUANTIDADE | sed -e 's/[,\.]//g')
		pad_number $(echo "${_MED} * ${_QUANTIDADE}" | bc) 18
	fi
	pad_number 0 13
	echo -n '0'
	echo -n '99991231'
	pad_number 1 7
	pad_number 0 13
	pad_text "BR${CODIGO}" 12
	pad_number 0 3

	echo
done 0<$BDI_TXT_FILE

rm -f $BDI_TXT_FILE

echo -n "99COTAHIST.AAAABOVESPA ${YEAR}${MONTH}${DAY}"
pad_number $((HOW_MANY_LINES+2)) 11
whites 203
echo

