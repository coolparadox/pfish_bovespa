#!/bin/sh
#

set -e

usage() {
	echo 'usage: pfish_bovespa_bdi_06_pdftotext bdi_06_AAAAMMDD.pdf' 1>&2
	exit 1
}

BDI_FILE="$1"
test -n "$BDI_FILE" || usage
shift
test $# -eq 0 || usage

pdftotext -enc ASCII7 -layout -nopgbrk "$BDI_FILE" - | \
sed -r \
	-e 's/  */ /g' \
	-e 's/^ *//' \
	-e 's/ *$//' \
	-e 's/^#//' \
	-e '/^LOTE-PADRAO/,$!d' \
	-e '/^CONCORDATARIAS/,$d' \
	-e '/Acoes do Ibovespa/d' \
	-e '/OFERTAS NEGS. REALIZADOS/d' \
	-e '/^COD. EMPRESA/d' \
	-e '/^B[[:digit:]]+$/d' \
	-e '/^LOTE-PADRAO/d' \
	-e '/^[[:blank:]]*$/d' \
	-e 's/ ([^[:digit:]/=+-])/_\1/g' \
	-e 's/_/ /'

