## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

nobase_include_HEADERS = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h

lib_LTLIBRARIES = libpfish_bovespa.la
libpfish_bovespa_la_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h pfish_bovespa.c revision_marker.h revision_marker.c
libpfish_bovespa_la_LDFLAGS = -version-info 0:0:0 -lpfish_syslog

bin_PROGRAMS = pfish_bovespa_library_info pfish_bovespa_database_init pfish_bovespa_file_import pfish_bovespa_stock_list pfish_bovespa_stock_history

pfish_bovespa_library_info_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h library_info.c 
pfish_bovespa_library_info_LDADD = -lpfish_syslog -lpfish_bovespa

pfish_bovespa_database_init_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h database_init.c
pfish_bovespa_database_init_LDADD = -lpfish_syslog -lpfish_bovespa

pfish_bovespa_file_import_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h file_import.c
pfish_bovespa_file_import_LDADD = -lpfish_syslog -lpfish_bovespa

pfish_bovespa_stock_list_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h stock_list.c
pfish_bovespa_stock_list_LDADD = -lpfish_syslog -lpfish_bovespa

pfish_bovespa_stock_history_SOURCES = pilot_fish/bovespa.h pilot_fish/bovespa_stdint.h stock_history.c
pfish_bovespa_stock_history_LDADD = -lpfish_syslog -lpfish_bovespa

CLEANFILES = $(bin_SCRIPTS)

MAINTAINERCLEANFILES = INSTALL Makefile.in aclocal.m4 config.guess config.sub config.h.in configure depcomp install-sh missing ltmain.sh *~ *.tar.*

maintainer-clean-local:
	-rm -rf m4

