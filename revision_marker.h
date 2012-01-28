/*
 * revision_marker.h
 * Database revison marker functions.
 */

#ifndef FILE_PFISH_BOVESPA_REVISION_MARKER_SEEN
#define FILE_PFISH_BOVESPA_REVISION_MARKER_SEEN


#define REVISION_MARKER_PATHNAME DBPATH "/.revision_marker"


/*
 * Build the expected content of a database revision marker file.
 *
 * @param[out] target dynamically allocated, null terminated string.
 *
 * @return 0 on success, negative on failure.
 */

int pfish_bovespa_revision_marker_content_alloc (char **target);


/*
 * Check the validity of the database revision marker.
 *
 * Or: verify if content of file REVISION_MARKER_PATHNAME matches
 * the output of pfish_bovespa_revision_marker_content_alloc().
 *
 * @return 0 if revision marker is valid, negative otherwise.
 */

int pfish_bovespa_revision_marker_check ();


#endif	// FILE_PFISH_BOVESPA_REVISION_MARKER_SEEN

