/*
 * revision_marker.c
 * Database revision marker functions.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include <pilot_fish/syslog.h>
#include <pilot_fish/syslog_macros.h>
#include <pilot_fish/bovespa.h>

#include "revision_marker.h"


#define FAILURE return (-1)
#define SUCCESS return (0)


int pfish_bovespa_revision_marker_content_alloc (char **target) {

	pfish_bovespa_library_info_t library_info;

	pfish_bovespa_library_info_get (&library_info);
	if ((asprintf (target, "%s\n%s\n%s\n", library_info.build_date, library_info.build_time, library_info.compiler_version)) < 0) {

		CRIT ("cannot build content string.");
		FAILURE;

	}
	SUCCESS;

}


int pfish_bovespa_revision_marker_check () {

	char *expected_content;
	char *current_content;
	size_t content_size;
	FILE *revision_marker_file;

	if ((revision_marker_file = fopen (REVISION_MARKER_PATHNAME, "r")) == NULL) {

		CRIT ("cannot open revision marker file in read mode.");
		FAILURE;

	}

#define FREE \
	fclose (revision_marker_file)

	if ((pfish_bovespa_revision_marker_content_alloc (&expected_content)) < 0) {

		CRIT ("cannot build expected content string.");
		FREE;
		FAILURE;

	}

#undef FREE
#define FREE \
	free (expected_content); \
	fclose (revision_marker_file)

	content_size = strlen (expected_content);
	if ((current_content = (char *) malloc (content_size)) == NULL) {

		EMERG ("cannot allocate %u bytes of heap space.", content_size);
		FREE;
		FAILURE;

	}

#undef FREE
#define FREE \
	free (current_content); \
	free (expected_content); \
	fclose (revision_marker_file)

	if ((fread (current_content, content_size, 1, revision_marker_file)) != 1) {

		CRIT ("cannot read revision marker file.");
		FREE;
		FAILURE;

	}
	if ((memcmp (current_content, expected_content, content_size)) != 0) {

		NOTICE ("database revision marker mismatch.");
		FREE;
		FAILURE;

	}
	DEBUG ("success.");
	FREE;
	SUCCESS;

#undef FREE

}


#undef SUCCESS
#undef FAILURE

