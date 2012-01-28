/*
 * pfish_bovespa.c
 *
 * pilot_fish bovespa daily quotes library.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <pilot_fish/syslog.h>
#include <pilot_fish/syslog_macros.h>

#include <pilot_fish/bovespa.h>

#include "revision_marker.h"


void pfish_bovespa_library_info_get (pfish_bovespa_library_info_t *target) {

	target->build_date = __DATE__;
	target->build_time = __TIME__;
	target->compiler_version = __VERSION__;

}


int pfish_bovespa_stock_list_alloc_selector (const struct dirent *dirent) {

	if (dirent->d_type != DT_REG) {

		return (0);

	}
	if (dirent->d_name[0] == '.') {

		return (0);

	}
	return (1);

}


pfish_bovespa_stock_list_t *pfish_bovespa_stock_list_alloc () {

	pfish_bovespa_stock_list_t *answer;	// The answer.
	size_t answer_size;	// Number of octets of the answer.

	struct dirent **namelist;	// List of stock files.
	int namelist_size;	// Size of stock file list.

	size_t i;	// Short term generic counter.

	/*
	 * Check database revision.
	 */

	if ((pfish_bovespa_revision_marker_check ()) < 0) {

		ALERT ("database revision mismatch; please reinitialize it.");
		return (NULL);

	}

	/*
	 * Scan all regular files of the database directory.
	 * Have the file list sorted.
	 */

	if ((namelist_size = scandir (DBPATH, &namelist, pfish_bovespa_stock_list_alloc_selector, alphasort)) < 0) {

		ERRNO_ERR;
		CRIT ("cannot scan directory '%s'.", DBPATH);
		return (NULL);

	}

	/*
	 * Compose the answer with the filenames of the scanned directory.
	 */

	answer_size = sizeof (size_t) + (sizeof (pfish_bovespa_stock_id_t) * namelist_size);
	if ((answer = (pfish_bovespa_stock_list_t *) malloc (answer_size)) == NULL) {

		EMERG ("cannot allocate %u octets from heap.", answer_size);
		return (NULL);

	}
	answer->stock_list_size = namelist_size;
	for ( i = 0; i < namelist_size; i++ ) {

		memset (answer->stock_list[i].id, 0, PFISH_BOVESPA_CODNEG_SIZE);
		strcpy (answer->stock_list[i].id, namelist[i]->d_name);

	}

	/*
	 * Once the answer is built, release the resources used by the scanning of the database directory.
	 */

	for ( i = 0; i < namelist_size; i++ ) {

		free (namelist[i]);

	}
	free (namelist);

	/*
	 * All set.
	 */

	return (answer);

}


#define SUCCESS return (0)
#define FAILURE return (-1)

int pfish_bovespa_stock_history_alloc (const pfish_bovespa_stock_id_t *stock_id, pfish_bovespa_stock_history_t **answer) {

	size_t dbpath_len;
	char stock_file_name[PATH_MAX];
	int stock_file_des;
	struct stat stock_file_stat;

	/*
	 * Check database revision.
	 */

	if ((pfish_bovespa_revision_marker_check ()) < 0) {

		ALERT ("database revision mismatch; please reinitialize it.");
		FAILURE;

	}

	/*
	 * Build the full pathname of the stock file.
	 */

	if ((snprintf (stock_file_name, PATH_MAX, "%s/%s", DBPATH, stock_id->id)) >= PATH_MAX) {

		ALERT ("pathname buffer overflow.");
		FAILURE;

	}
	DEBUG ("stock_file_name = '%s'", stock_file_name);

	/*
	 * "Just" mmap.
	 */

	if ((stock_file_des = open (stock_file_name, O_RDONLY | O_EXCL)) < 0) {

		switch (errno) {

			case ENOENT:

				/*
				 * Stock file does not exist in database directory.
				 * By definition this is not a failure.
				 */

				DEBUG("stock '%s' does not exist in database.", stock_id->id);
				*answer = NULL;
				SUCCESS;

			default:

				ERRNO_ERR;
				CRIT ("cannot open file '%s'.", stock_file_name);
				FAILURE;

		}

	}
	if ((fstat (stock_file_des, &stock_file_stat)) < 0) {

		ERRNO_ERR;
		CRIT ("cannot stat file descriptor '%d'.", stock_file_des);
		FAILURE;

	}
	if ((*answer = (pfish_bovespa_stock_history_t *) mmap (NULL, stock_file_stat.st_size, PROT_READ, MAP_PRIVATE, stock_file_des, 0)) == (pfish_bovespa_stock_history_t *) (-1)) {

		ERRNO_ERR;
		CRIT ("cannot memory-map file descriptor '%d'.", stock_file_des);
		FAILURE;

	}
	if ((close (stock_file_des)) < 0) {

		ERRNO_ERR;
		WARNING ("cannot close file descriptor '%d'.", stock_file_des);

	}

	/*
	 * All set.
	 */

	SUCCESS;

}


int pfish_bovespa_stock_history_free (pfish_bovespa_stock_history_t *target) {

#define LENGTH ((2 * sizeof (size_t)) + (target->daily_quotes_size * sizeof (pfish_bovespa_daily_quote_t)))

	if ((munmap (target, LENGTH)) < 0) {

		CRIT ("cannot memory-unmap stock file.");
		FAILURE;

	}
	SUCCESS;

#undef LENGTH

}


#undef FAILURE
#undef SUCCESS

