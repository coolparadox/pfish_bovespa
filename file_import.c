/*
 * file_import.c
 *
 * Import a Bovespa file into pilot_fish Bovespa history database.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <argp.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>
#include <assert.h>

#include <pilot_fish/syslog.h>
#include <pilot_fish/syslog_macros.h>

#include <pilot_fish/bovespa.h>


/*
 * Command line argument parsing.
 */

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "pfish_bovespa_file_import -- import a Bovespa file into the pilot_fish bovespa database.\vThe bovespa file is read from standard input.\nHistory stock data previously existent in the database is overwritten on data timestamp collision.\n";

static struct argp argp = { 0, 0, 0, doc };


/*
 * Constraints from layout specs of Bovespa files.
 */

/* Maximum number of useful characters of a line (350) + extra buffer. */

#define BOVESPA_REGISTER_LENGTH 0x200

/* Types of Bovespa files. */

#define BOVESPA_FILE_TYPE_HIST 0
#define BOVESPA_FILE_TYPE_BDIN 1

/* Sections of Bovespa files. */

#define BOVESPA_FILE_SECTION_HEADER 0
#define BOVESPA_FILE_SECTION_QUOTES 1
#define BOVESPA_FILE_SECTION_TRAILER 2
#define BOVESPA_FILE_SECTION_OTHER 3


/*
 * Positional mapping of register fields inside Bovespa files.
 *
 * HIST_* are subsets of the spec 'SeriesHistoricas_Layout.pdf'.
 * BDIN_* are subsets of the spec 'BDIN_Bovespa_v11.pdf'.
 *
 * N, X, V99 are subsets of field types of the specs above.
 * Although they mean the same here (that is, text), it was helpful 
 * differentiating among them for copying purposes.
 */

#define N(FIELD_NAME,FROM,TO) BOVESPA_FIELD (FIELD_NAME, FROM, TO)
#define X(FIELD_NAME,FROM,TO) BOVESPA_FIELD (FIELD_NAME, FROM, TO)
#define V99(FIELD_NAME,FROM,TO) BOVESPA_FIELD (FIELD_NAME, FROM, TO)

#define HIST_HEADER_REGISTER \
	X (nome_arquivo, 3, 15); \
	X (codigo_origem, 16, 23); \
	N (data_geracao, 24, 31)

#define BDIN_HEADER_REGISTER \
	X (nome_arquivo, 3, 10); \
	X (codigo_origem, 11, 18); \
	N (codigo_destino, 19, 22); \
	N (data_geracao, 23, 30); \
	N (ano_pregao, 31, 34); \
	N (mes_pregao, 35, 36); \
	N (dia_pregao, 37, 38); \
	N (hora_geracao, 39, 42)

#define HIST_QUOTE_REGISTER \
	N (ano_pregao, 3, 6); \
	N (mes_pregao, 7, 8); \
	N (dia_pregao, 9, 10); \
	X (cod_bdi, 11, 12); \
	X (cod_neg, 13, 24); \
	N (tp_merc, 25, 27); \
	X (nom_res, 28, 39); \
	X (especi, 40, 49); \
	X (mod_ref, 53, 56); \
	V99 (pre_abe, 57, 69); \
	V99 (pre_max, 70, 82); \
	V99 (pre_min, 83, 95); \
	V99 (pre_med, 96, 108); \
	V99 (pre_ult, 109, 121); \
	N (tot_neg, 148, 152); \
	N (qua_tot, 153, 170); \
	V99 (vol_tot, 171, 188); \
	N (fat_cot, 211, 217); \
	X (cod_isi, 231, 242)

#define BDIN_QUOTE_REGISTER \
	X (cod_bdi, 3, 4); \
	X (cod_neg, 58, 69); \
	N (tp_merc, 70, 72); \
	X (nom_res, 35, 46); \
	X (especi, 47, 56); \
	V99 (pre_abe, 91, 101); \
	V99 (pre_max, 102, 112); \
	V99 (pre_min, 113, 123); \
	V99 (pre_med, 124, 134); \
	V99 (pre_ult, 135, 145); \
	N (tot_neg, 174, 178); \
	N (qua_tot, 179, 193); \
	V99 (vol_tot, 194, 210); \
	N (fat_cot, 246, 252); \
	X (cod_isi, 266, 277)

#define HIST_TRAILER_REGISTER \
	X (nome_arquivo, 3, 15); \
	X (codigo_origem, 16, 23); \
	N (data_geracao, 24, 31); \
	N (total_registros, 32, 42)

#define BDIN_TRAILER_REGISTER \
	X (nome_arquivo, 3, 10); \
	X (codigo_origem, 11, 18); \
	N (codigo_destino, 19, 22); \
	N (data_geracao, 23, 30); \
	N (total_registros, 31, 39)


/*
 * Information containers of important registers (lines) of Bovespa files.
 */

#define BOVESPA_FIELD(FIELD_NAME,FROM,TO) char FIELD_NAME[TO - FROM + 2]

struct hist_header_register {

	HIST_HEADER_REGISTER;

};

struct hist_quote_register {

	HIST_QUOTE_REGISTER;

};

struct hist_trailer_register {

	HIST_TRAILER_REGISTER;

};

struct bdin_header_register {

	BDIN_HEADER_REGISTER;

};

struct bdin_quote_register {

	BDIN_QUOTE_REGISTER;

};

struct bdin_trailer_register {

	BDIN_TRAILER_REGISTER;

};

union bovespa_header_register {

	struct hist_header_register hist;
	struct bdin_header_register bdin;

};

union bovespa_quote_register {

	struct hist_quote_register hist;
	struct bdin_quote_register bdin;

};

union bovespa_trailer_register {

	struct hist_trailer_register hist;
	struct bdin_trailer_register bdin;

};

#undef BOVESPA_FIELD


/*
 * Discover the type of a Bovespa file.
 *
 * @param[in] header_register first line of a Bovespa file.
 * @param[out] answer one of BOVESPA_FILE_TYPE_* values.
 *
 * @return 0 on success, negative on failure.
 */

int discover_file_type (const char *header_register, unsigned int *answer);


/*
 * Discover the type of a Bovespa register.
 *
 * @param[in] file_type one of BOVESPA_FILE_TYPE_* values.
 * @param[in] bovespa_register a line of a Bovespa file.
 * @param[out] answer one of BOVESPA_FILE_SECTION_* values.
 *
 * @return 0 on success, negative on failure.
 */

int discover_register_type (unsigned int file_type, const char *bovespa_register, unsigned int *answer);


/*
 * Sanitize a field of a Bovespa register.
 *
 * @param[in,out] field field to be sanitized, will contain the result of sanitization..
 * @param[in] field_size size of the field.
 */

void sanitize_field (char *field, size_t field_size);


/*
 * Mapper structure for fields of a Bovespa register.
 */

struct bovespa_mapper {

	char *ano_pregao;
	char *mes_pregao;
	char *dia_pregao;
	char *cod_bdi;
	char *cod_neg;
	char *tp_merc;
	char *nom_res;
	char *especi;
	char *mod_ref;
	char *pre_abe;
	char *pre_max;
	char *pre_min;
	char *pre_med;
	char *pre_ult;
	char *tot_neg;
	char *qua_tot;
	char *vol_tot;
	char *fat_cot;
	char *cod_isi;

};

typedef struct bovespa_mapper bovespa_mapper_t;


/*
 * Wrapping structure to make possible for a daily quote of a specific stock to be part of a linked list.
 */

typedef struct quote_node quote_node_t;

struct quote_node {

	pfish_bovespa_stock_id_t stock;
	pfish_bovespa_daily_quote_t quote;
	quote_node_t *next;

};


/*
 * Decide if a Bovespa mapping is useful. If so, transform it to a quote node, and then add it to the quotes linked list.
 *
 * @param[in] mapper mapper structure of Bovespa textual fields containing quote information.
 * @param[in,out] last last node of the quotes linked list, will contain the new last node on successfull append.
 *
 * @return 0 on successful append, other positive on ignored node, negative on failure.
 */

int quotes_list_append (const bovespa_mapper_t *mapper, quote_node_t **last);


/*
 * Compare two quote nodes.
 * Arguments type hint: (const void *) == (quote_node_t **)
 *
 * @param a first quote node.
 * @param b second quote node.
 *
 * @return negative if a < b, positive if a > b, zero if a == b.
 */

int compare_quote_nodes (const void *a, const void *b);


/*
 * Merge two arrays of (pointers to) daily quotes.
 *
 * @param[in] a first array of pointers to daily quotes.
 * @param[in] a_size how many elements in 'a'.
 * @param[in] b second array of pointers to daily quotes.
 * @param[in] b_size how many elements in 'b'.
 * @param[out] answer merged array.
 * @param[out] answer_size how many elements in 'answer'.
 *
 * Input arrays are suposed to be sorted by trading date.
 * Output array is sorted by trading date.
 * If there are elements with same trading date on both input arrays, the elements of the second array will prevail.
 *
 * @return 0 on success, negative on failure.
 */

int merge_daily_quotes (pfish_bovespa_daily_quote_t **a, size_t a_size, pfish_bovespa_daily_quote_t **b, size_t b_size, pfish_bovespa_daily_quote_t ***answer, size_t *answer_size);


/*
 * The portal.
 */

#define SUCCESS return (EXIT_SUCCESS)
#define FAILURE return (EXIT_FAILURE)

int main (int argc, char **argv) {

	int rcode;		// Return code of functions.
	size_t i;		// General purpose short ranged unsigned counter.
	unsigned int aux_ul;	// General purpose short ranged unsigned integer.
	char *aux_charp;	// General purpose short ranged character pointer.

	char bovespa_register[BOVESPA_REGISTER_LENGTH+2];	// A line of Bovespa files.
	unsigned int file_type;		// Type of Bovespa file being processed.
	unsigned int file_section;	// Current section of the Bovespa file being processed.
	unsigned int register_type;	// Type of the current Bovespa register being processed.
	size_t register_count;		// How many Bovespa registers were processed so far.

	bovespa_mapper_t mapper;	// Bovespa field mapper.

	union bovespa_header_register header_register;		// A Bovespa header register.
	union bovespa_quote_register quote_register;		// A Bovespa quote register.
	union bovespa_trailer_register trailer_register;	// A Bovespa trailer register.

	quote_node_t *quotes_list_first;	// First node of the quotes linked list.
	quote_node_t *quotes_list_last;		// Last node of the quotes linked list.
	size_t quotes_list_count;	// How many elements in the quotes linked list.

	quote_node_t **quotes_array;	// Array of pointers to quote nodes.
	size_t quotes_index;	// Quotes array indexer in search loops.
	char current_stock_id[PFISH_BOVESPA_CODNEG_SIZE];	// Helps to find new stocks in the quotes array search loop.
	size_t stock_count;	// How many stocks were processed.
	size_t quote_history_size;	// Size of the history sequence of a stock in the quotes array.

	pfish_bovespa_stock_history_t *database_stock_history;

	pfish_bovespa_daily_quote_t **new_daily_quotes;		// Array of pointers to daily quotes from Bovespa file.
	pfish_bovespa_daily_quote_t **database_daily_quotes;	// Array of pointers to daily quotes from the database.
	pfish_bovespa_daily_quote_t **merged_daily_quotes;	// Result of the merge of new_daily_quotes with database_daily_quotes.
	size_t merged_daily_quotes_size;	// Size of the merged array of daily quotes.
	size_t last_xplit;	// Index of 'merged_daily_quotes' of the most recent inplit or split of the stock.
	regex_t xplit_regex;	// Compiled regular expression for help finding stock inplits or splits.
	int xplit_match_previous;	// Reminder if xplit_regex matched at the previous loop pass.

	FILE *stock_file;	// Stream to the stock file currently being built.
	char stock_pathname[PATH_MAX];	// Pathname of the stock file currently being built.
	char stock_backup_pathname[PATH_MAX];	// Pathname of the backup file of the stock currently being built.


	/*
	 * Begin.
	 */

	pfish_syslog_init (SYSLOG_FACILITY, LOG_PERROR | LOG_PID);
	DEBUG ("start.");

	/*
	 * Parse command line arguments.
	 */

	argp_parse (&argp, argc, argv, 0, 0, 0);

	/*
	 * Initialize the quotes linked list.
	 */

	quotes_list_first = NULL;
	quotes_list_last = NULL;
	quotes_list_count = 0;

	/*
	 * Iterate through all registers (lines) of the Bovespa file.
	 */

	register_count = 0;
	DEBUG ("entering header section.");
	file_section = BOVESPA_FILE_SECTION_HEADER;
	while ((fgets (bovespa_register, BOVESPA_REGISTER_LENGTH, stdin)) != NULL) {

		DEBUG ("bovespa register = '%s'", bovespa_register);
		if (register_count == 0) {

			/* 
			 * First register of the Bovespa file. 
			 */

			if ((discover_file_type (bovespa_register, &file_type)) != 0) {

				CRIT ("cannot discover the type of the bovespa file.");
				FAILURE;

			}
			DEBUG ("bovespa file type = '%u'.", file_type);

			/* 
			 * Adapt the Bovespa field mapper structure according to the file type.
			 */

			switch (file_type) {

				case BOVESPA_FILE_TYPE_HIST:

					mapper.ano_pregao = quote_register.hist.ano_pregao;
					mapper.mes_pregao = quote_register.hist.mes_pregao;
					mapper.dia_pregao = quote_register.hist.dia_pregao;
					mapper.cod_bdi = quote_register.hist.cod_bdi;
					mapper.cod_neg = quote_register.hist.cod_neg;
					mapper.tp_merc = quote_register.hist.tp_merc;
					mapper.nom_res = quote_register.hist.nom_res;
					mapper.especi = quote_register.hist.especi;
					mapper.mod_ref = quote_register.hist.mod_ref;
					mapper.pre_abe = quote_register.hist.pre_abe;
					mapper.pre_max = quote_register.hist.pre_max;
					mapper.pre_min = quote_register.hist.pre_min;
					mapper.pre_med = quote_register.hist.pre_med;
					mapper.pre_ult = quote_register.hist.pre_ult;
					mapper.tot_neg = quote_register.hist.tot_neg;
					mapper.qua_tot = quote_register.hist.qua_tot;
					mapper.vol_tot = quote_register.hist.vol_tot;
					mapper.fat_cot = quote_register.hist.fat_cot;
					mapper.cod_isi = quote_register.hist.cod_isi;
					break;

				case BOVESPA_FILE_TYPE_BDIN:

					mapper.ano_pregao = header_register.bdin.ano_pregao;
					mapper.mes_pregao = header_register.bdin.mes_pregao;
					mapper.dia_pregao = header_register.bdin.dia_pregao;
					mapper.cod_bdi = quote_register.bdin.cod_bdi;
					mapper.cod_neg = quote_register.bdin.cod_neg;
					mapper.tp_merc = quote_register.bdin.tp_merc;
					mapper.nom_res = quote_register.bdin.nom_res;
					mapper.especi = quote_register.bdin.especi;
					mapper.mod_ref = "R$";
					mapper.pre_abe = quote_register.bdin.pre_abe;
					mapper.pre_max = quote_register.bdin.pre_max;
					mapper.pre_min = quote_register.bdin.pre_min;
					mapper.pre_med = quote_register.bdin.pre_med;
					mapper.pre_ult = quote_register.bdin.pre_ult;
					mapper.tot_neg = quote_register.bdin.tot_neg;
					mapper.qua_tot = quote_register.bdin.qua_tot;
					mapper.vol_tot = quote_register.bdin.vol_tot;
					mapper.fat_cot = quote_register.bdin.fat_cot;
					mapper.cod_isi = quote_register.bdin.cod_isi;
					break;

				default:

					CRIT ("unknown bovespa file type '%u'.", file_type);
					FAILURE;

			}

		}
		register_count++;
		if (discover_register_type (file_type, bovespa_register, &register_type) < 0) {

			CRIT ("cannot discover the type of the bovespa register.");
			FAILURE;

		}
		DEBUG ("bovespa register type = '%u'.", register_type);

		/* 
		 * Adapt to the current section of the Bovespa file.
		 */

		switch (file_section) {

			case BOVESPA_FILE_SECTION_HEADER:

				if (register_type != BOVESPA_FILE_SECTION_HEADER) {

					CRIT ("missing header register.");
					FAILURE;

				}

#define BOVESPA_FIELD(FIELD_NAME,FROM,TO) \
	memcpy (UNION_NAME.STRUCT_NAME.FIELD_NAME, &bovespa_register[FROM - 1], TO - FROM + 1); \
	UNION_NAME.STRUCT_NAME.FIELD_NAME[TO - FROM + 1] = 0; \
	DEBUG (#FIELD_NAME " = '%s'", UNION_NAME.STRUCT_NAME.FIELD_NAME); \
	sanitize_field (UNION_NAME.STRUCT_NAME.FIELD_NAME, TO - FROM + 1); \
	DEBUG ("sanitized " #FIELD_NAME " = '%s'", UNION_NAME.STRUCT_NAME.FIELD_NAME)

#define UNION_NAME header_register

#define VERIFY_HEADER_GARBAGE \
	if ((strcmp (UNION_NAME.STRUCT_NAME.codigo_origem, "BOVESPA")) != 0) { \
		CRIT ("heading garbage detected."); \
		FAILURE; \
	}

#define INFO_HEADER_FIELD(FIELD_NAME,FIELD_TEXT) \
	INFO (FIELD_TEXT " = '%s'.", UNION_NAME.STRUCT_NAME.FIELD_NAME)

				/* 
				 * Copy fields of bovespa register to the header structure.
				 */

				switch (file_type) {

					case BOVESPA_FILE_TYPE_HIST:

#define STRUCT_NAME hist

						HIST_HEADER_REGISTER;
						VERIFY_HEADER_GARBAGE;
						INFO_HEADER_FIELD (nome_arquivo, "nome de arquivo");
						INFO_HEADER_FIELD (data_geracao, "data de geração");

#undef STRUCT_NAME

						break;

					case BOVESPA_FILE_TYPE_BDIN:

#define STRUCT_NAME bdin

						BDIN_HEADER_REGISTER;
						VERIFY_HEADER_GARBAGE;
						INFO_HEADER_FIELD (nome_arquivo, "nome de arquivo");
						INFO_HEADER_FIELD (data_geracao, "data de geração");
						INFO_HEADER_FIELD (ano_pregao, "ano de geração");
						INFO_HEADER_FIELD (mes_pregao, "mês de geração");
						INFO_HEADER_FIELD (dia_pregao, "dia de geração");
						INFO_HEADER_FIELD (hora_geracao, "hora de geração");

#undef STRUCT_NAME

						break;

					default:

						CRIT ("unknown bovespa file type '%u'.", file_type);
						FAILURE;

				}

#undef INFO_HEADER_FIELD
#undef VERIFY_HEADER_GARBAGE

#undef UNION_NAME

				file_section = BOVESPA_FILE_SECTION_QUOTES;
				break;

			case BOVESPA_FILE_SECTION_QUOTES:

				switch (register_type) {

					case BOVESPA_FILE_SECTION_HEADER:

						CRIT ("duplicate header register.");
						FAILURE;

					case BOVESPA_FILE_SECTION_QUOTES:

#define UNION_NAME quote_register

						/* 
						 * Copy fields of bovespa register to the quote structure.
						 */

						switch (file_type) {

							case BOVESPA_FILE_TYPE_HIST:

#define STRUCT_NAME hist

								HIST_QUOTE_REGISTER;
								break;

#undef STRUCT_NAME

							case BOVESPA_FILE_TYPE_BDIN:

#define STRUCT_NAME bdin

								BDIN_QUOTE_REGISTER;
								break;

#undef STRUCT_NAME

							default:

								CRIT ("unknown bovespa file type '%u'.", file_type);
								FAILURE;

						}

#undef UNION_NAME

						/* 
						 * Append quote register data to the quotes linked list.
						 */

						if ((rcode = quotes_list_append (&mapper, &quotes_list_last)) < 0) {

							CRIT ("cannot append Bovespa data to the quotes list.");
							FAILURE;

						}
						if (quotes_list_first == NULL) {

							quotes_list_first = quotes_list_last;

						}
						if (rcode == 0) {

							quotes_list_count += 1;

						}

						break;

					case BOVESPA_FILE_SECTION_OTHER:

						break;

					case BOVESPA_FILE_SECTION_TRAILER:

						DEBUG ("trailer detected.");

#define UNION_NAME trailer_register

						/* 
						 * Copy fields of bovespa register to the trailer structure.
						 */

						switch (file_type) {

							case BOVESPA_FILE_TYPE_HIST:

#define STRUCT_NAME hist

								HIST_TRAILER_REGISTER;
								break;

#undef STRUCT_NAME

							case BOVESPA_FILE_TYPE_BDIN:

#define STRUCT_NAME bdin

								BDIN_TRAILER_REGISTER;
								break;

#undef STRUCT_NAME

							default:

								CRIT ("unknown bovespa file type '%u'.", file_type);
								FAILURE;

						}

#undef UNION_NAME
#undef BOVESPA_FIELD

						/* 
						 * Final sanity verifications about the Bovespa file.
						 */

#define MATCH_TRAILER_FIELD(STRUCT_NAME,FIELD_NAME) \
	if ((strcmp (header_register.STRUCT_NAME.FIELD_NAME, trailer_register.STRUCT_NAME.FIELD_NAME)) != 0) { \
		CRIT ("trailer field mismatch (field = '" #FIELD_NAME "', header = '%s', trailer = '%s')", header_register.STRUCT_NAME.FIELD_NAME, trailer_register.STRUCT_NAME.FIELD_NAME); \
		FAILURE; \
	}

#define VERIFY_REGISTER_COUNT(STRUCT_NAME) \
	errno = 0; \
	aux_ul = strtoul (trailer_register.STRUCT_NAME.total_registros, &aux_charp, 10); \
	if (*aux_charp != 0) { \
		ERRNO_ERR; \
		ERR ("cannot understand '%s' as an unsigned integer.", trailer_register.STRUCT_NAME.total_registros); \
		CRIT ("cannot verify bovespa register count."); \
		FAILURE; \
	} \
	if (register_count != aux_ul) { \
		ERR ("number of registers (%u) not equal to trailer's register count field (%u)", register_count, aux_ul); \
		CRIT ("bovespa register count mismatch."); \
		FAILURE; \
	}

						switch (file_type) {

							case BOVESPA_FILE_TYPE_HIST:

								MATCH_TRAILER_FIELD (hist, nome_arquivo);
								MATCH_TRAILER_FIELD (hist, codigo_origem);
								MATCH_TRAILER_FIELD (hist, data_geracao);
								VERIFY_REGISTER_COUNT (hist);
								break;

							case BOVESPA_FILE_TYPE_BDIN:

								MATCH_TRAILER_FIELD (bdin, nome_arquivo);
								MATCH_TRAILER_FIELD (bdin, codigo_origem);
								MATCH_TRAILER_FIELD (bdin, codigo_destino);
								MATCH_TRAILER_FIELD (bdin, data_geracao);
								VERIFY_REGISTER_COUNT (bdin);
								break;

							default:

								CRIT ("unknown bovespa file type '%u'.", file_type);
								FAILURE;

						}

#undef MATCH_TRAILER_FIELD
#undef VERIFY_REGISTER_COUNT

						file_section = BOVESPA_FILE_SECTION_TRAILER;
						break;

				}
				break;


			case BOVESPA_FILE_SECTION_TRAILER:

				CRIT ("trailing garbage detected.");
				FAILURE;

			default:

				CRIT ("unknown bovespa register type '%u'.", register_type);
				FAILURE;

		}

	}
	if ((ferror (stdin)) != 0) {

		ERRNO_ERR;
		CRIT ("cannot read standard input.");
		FAILURE;

	}
	INFO ("%u daily quotes parsed from bovespa file.", quotes_list_count);

	/*
	 * End of reading of Bovespa file.
	 * At this point, 'quotes_list_first' is the start of a linked list containing all interesting data, 
	 * and 'quotes_list_count' is the number of nodes of this list.
	 */

	/*
	 * For ease of sorting, let's mount an array of pointers to the list's data.
	 */
	if ((quotes_array = (quote_node_t **) malloc (quotes_list_count * sizeof (quote_node_t *))) == NULL) {

		ALERT ("cannot allocate %u bytes of heap space.", quotes_list_count * sizeof (quote_node_t *));
		FAILURE;

	}

#define aux_nodep quotes_list_last

	aux_nodep = quotes_list_first;
	i = 0;
	while (aux_nodep != NULL) {

		quotes_array[i++] = aux_nodep;
		aux_nodep = aux_nodep->next;

	}
	assert (i == quotes_list_count);

#undef aux_nodep

	/*
	 * Sort the array of quotes.
	 */

	qsort (quotes_array, quotes_list_count, sizeof (quote_node_t *), compare_quote_nodes);
	DEBUG ("quotes sorted.");

	/*
	 * Compile a regular expression to help find out inplits and splits.
	 */

#define XPLIT_PATTERN "E.?[BG] *"

	if ((regcomp (&xplit_regex, XPLIT_PATTERN, REG_EXTENDED | REG_NOSUB)) != 0) {

		ALERT ("cannot compile extended regular expression '%s'.", XPLIT_PATTERN);
		FAILURE;

	}

#undef XPLIT_PATTERN

	/*
	 * Find all sequences of quote history for each stock in que quotes array.
	 * This code assumes that the primary sorting key is the stock id.
	 */

	DEBUG ("scanning quotes array.");
	memset (current_stock_id, 0, PFISH_BOVESPA_CODNEG_SIZE);
	stock_count = 0;
	for ( quotes_index = 0; quotes_index < quotes_list_count; quotes_index++ ) {

		if ((memcmp (quotes_array[quotes_index]->stock.id, current_stock_id, PFISH_BOVESPA_CODNEG_SIZE)) != 0) {

			/*
			 * New stock found.
			 */

			DEBUG ("found stock '%s'.", quotes_array[quotes_index]->stock.id);
			memcpy (current_stock_id, quotes_array[quotes_index]->stock.id, PFISH_BOVESPA_CODNEG_SIZE);
			stock_count++;

			/*
			 * Seek forward to find the length of the history sequence for this stock.
			 */

			for ( i = quotes_index + 1; i < quotes_list_count; i++ ) {

				if ((memcmp (quotes_array[i]->stock.id, current_stock_id, PFISH_BOVESPA_CODNEG_SIZE)) != 0) {

					break;

				}

			}
			quote_history_size = i - quotes_index;

			/*
			 * The history sequence is a sequence of pointers to nodes of the linked list (quote_node_t).
			 * Transform it to pointers to daily quotes (pfish_bovespa_daily_quote_t).
			 */

			if ((new_daily_quotes = (pfish_bovespa_daily_quote_t **) malloc (quote_history_size * sizeof (pfish_bovespa_daily_quote_t *))) == NULL) {

				ALERT ("cannot allocate '%u' butes of heap space.", quote_history_size * sizeof (pfish_bovespa_daily_quote_t *));
				FAILURE;

			}
			for ( i = 0; i < quote_history_size; i++ ) {

				new_daily_quotes[i] = &(quotes_array[quotes_index + i]->quote);

			}

			/*
			 * Retrieve from database an array of pointers to the current daily quotes of this stock.
			 */

			if ((pfish_bovespa_stock_history_alloc (&(quotes_array[quotes_index]->stock), &database_stock_history)) < 0) {

				CRIT ("cannot retrieve history of stock '%s' from the database.", current_stock_id);
				FAILURE;

			}
			if (database_stock_history != NULL) {

				if ((database_daily_quotes = (pfish_bovespa_daily_quote_t **) malloc (database_stock_history->daily_quotes_size * sizeof (pfish_bovespa_daily_quote_t *))) == NULL) {

					ALERT ("cannot allocate '%u' butes of heap space.", database_stock_history->daily_quotes_size * sizeof (pfish_bovespa_daily_quote_t *));
					FAILURE;

				}
				for ( i = 0; i < database_stock_history->daily_quotes_size; i++ ) {

					database_daily_quotes[i] = &(database_stock_history->daily_quotes[i]);

				}

			}
			else {

				database_daily_quotes = NULL;

			}

			/*
			 * Merge database and new daily quotes.
			 */

			if (database_daily_quotes != NULL) {

				if ((merge_daily_quotes (database_daily_quotes, database_stock_history->daily_quotes_size, new_daily_quotes, quote_history_size, &merged_daily_quotes, &merged_daily_quotes_size)) < 0) {

					CRIT ("cannot merge daily quotes.");
					FAILURE;

				}

			}
			else {

				if ((merged_daily_quotes = (pfish_bovespa_daily_quote_t **) malloc (quote_history_size * sizeof (pfish_bovespa_daily_quote_t *))) == NULL) {

					ALERT ("cannot allocate %u bytes of heap space.", quote_history_size * sizeof (pfish_bovespa_daily_quote_t *));
					FAILURE;

				}
				memcpy (merged_daily_quotes, new_daily_quotes, quote_history_size * sizeof (pfish_bovespa_daily_quote_t *));
				merged_daily_quotes_size = quote_history_size;

			}

			/*
			 * Detect most recent inplit or split of the stock.
			 */

#define xplit_match_current rcode

			// Search the merged array backwards until the first detection.
			// Obs.: logic of flags 'xplit_match_*' is negative.

			xplit_match_previous = 1;
			xplit_match_current = 1;

#define XPLIT_MATCH_PREVIOUS (xplit_match_previous == 0)
#define XPLIT_MATCH_CURRENT (xplit_match_current == 0)
#define XPLIT_DETECTED (XPLIT_MATCH_PREVIOUS && !XPLIT_MATCH_CURRENT)

			i = merged_daily_quotes_size;
			do {

				i--;
				xplit_match_previous = xplit_match_current;
				DEBUG ("stock %s, spec '%s', array pos %u", current_stock_id, merged_daily_quotes[i]->stock_spec, i);
				if ((xplit_match_current = regexec (&xplit_regex, merged_daily_quotes[i]->stock_spec, 0, NULL, 0)) != 0) {

					switch (xplit_match_current) {

						case REG_NOMATCH:

							break;

						default:

							CRIT ("cannot match xplit regexp with spec string of stock '%s'.", current_stock_id);
							FAILURE;

					}

				}
				if (i == 0) {

					break;

				}

			} while (!XPLIT_DETECTED);

#define LAST_XPLIT_POS (i + 1)

			if (XPLIT_DETECTED) {

				if ((database_stock_history == NULL) || (database_stock_history->last_xplit != LAST_XPLIT_POS)) {

					INFO ("inplit / split detected in stock '%s' at array position %u.", current_stock_id, LAST_XPLIT_POS);

				}
				last_xplit = LAST_XPLIT_POS;

			}
			else {

				last_xplit = 0;

			}

#undef LAST_XPLIT_POS
#undef XPLIT_MATCH_CURRENT
#undef XPLIT_MATCH_PREVIOUS
#undef XPLIT_DETECTED

#undef xplit_match_current

			/*
			 * At this point:
			 *
			 * 	- the stock being processed is identified by 'current_stock_id'.
			 * 	- the updated history of daily quotes of this stock is defined by 'merged_daily_quotes' and 'merged_daily_quotes_size'.
			 * 	- the last inplit or split of the stock is pointed by the index 'last_xplit'.
			 *
			 * No more information needed; let's build the stock history file.
			 */

#define STOCK_TEMP_PATHNAME DBPATH "/.stock.tmp"

			if ((stock_file = fopen (STOCK_TEMP_PATHNAME, "w")) == NULL) {

				ERRNO_ERR;
				CRIT ("cannot open file '%s' in write mode.", STOCK_TEMP_PATHNAME);
				FAILURE;

			}
			
			/*
			 * The stock file is a dump of a 'pfish_bovespa_stock_history_t' instance.
			 */

			if ((fwrite (&merged_daily_quotes_size, sizeof (size_t), 1, stock_file)) != 1) {

				ERRNO_ERR;
				CRIT ("cannot write field '%s' to temporary stock file.", "daily_quotes_size");
				FAILURE;

			}
			if ((fwrite (&last_xplit, sizeof (size_t), 1, stock_file)) != 1) {

				ERRNO_ERR;
				CRIT ("cannot write field '%s' to temporary stock file.", "last_xplit");
				FAILURE;

			}
			for ( i = 0; i < merged_daily_quotes_size; i++ ) {

				if ((fwrite (merged_daily_quotes[i], sizeof (pfish_bovespa_daily_quote_t), 1, stock_file)) != 1) {

					ERRNO_ERR;
					CRIT ("cannot write field '%s[%u]' to temporary stock file.", "daily_quotes", i);
					FAILURE;

				}

			}
			if ((fclose (stock_file)) != 0) {

				ERRNO_ERR;
				CRIT ("cannot close temporary stock file '%s'.", STOCK_TEMP_PATHNAME);
				FAILURE;

			}

			/*
			 * Stock file built, but in a temporary name.
			 * Make the file official.
			 */

			// Detach database_stock_history from its database file.
			// Release other uneeded resources.

			if (database_daily_quotes != NULL) {

				free (database_daily_quotes);

			}
			if (database_stock_history != NULL) {

				if ((pfish_bovespa_stock_history_free (database_stock_history)) < 0) {

					CRIT ("cannot release history of stock '%s'.", current_stock_id);
					FAILURE;

				}

			};
			free (new_daily_quotes);

			// Here I play with a backup file to maintain data existence at all times.

			if ((snprintf (stock_pathname, PATH_MAX, "%s/%s", DBPATH, current_stock_id)) >= PATH_MAX) {

				CRIT ("cannot build pathname of database file for stock '%s'.", current_stock_id);
				FAILURE;

			}
			if ((snprintf (stock_backup_pathname, PATH_MAX, "%s/.%s", DBPATH, current_stock_id)) >= PATH_MAX) {

				CRIT ("cannot build pathname of database backup file for stock '%s'.", current_stock_id);
				FAILURE;

			}
			if ((unlink (stock_backup_pathname)) != 0) {

				switch (errno) {

					case ENOENT:

						break;

					default:

						ERRNO_ERR;
						CRIT ("cannot erase stock backup file '%s'.", stock_backup_pathname);
						FAILURE;

				}

			}
			if ((rename (stock_pathname, stock_backup_pathname)) == -1) {

				switch (errno) {

					case ENOENT:

						break;

					default:

						ERRNO_ERR;
						CRIT ("cannot move stock file '%s' to backup file.", stock_pathname);
						FAILURE;

				}

			}
			if ((rename (STOCK_TEMP_PATHNAME, stock_pathname)) == -1) {

				ERRNO_ERR;
				CRIT ("cannot move temporary stock file '%s' to official file for stock '%s'.", STOCK_TEMP_PATHNAME, current_stock_id);
				FAILURE;

			}
			if ((unlink (stock_backup_pathname)) != 0) {

				switch (errno) {

					case ENOENT:

						break;

					default:

						ERRNO_ERR;
						CRIT ("cannot erase stock backup file '%s'.", stock_backup_pathname);
						FAILURE;

					}

			}

#undef STOCK_TEMP_PATHNAME

			/*
			 * Stock resource releasing.
			 */

			free (merged_daily_quotes);

		}

	}

	/*
	 * Global resource releasing.
	 */

	regfree (&xplit_regex);

	/*
	 * End.
	 */

	INFO ("%u stocks processed.", stock_count);
	SUCCESS;

}

#undef FAILURE
#undef SUCCESS


#define SUCCESS return (0)
#define FAILURE return (-1)


int discover_file_type (const char *header_register, unsigned int *answer) {

	if ((memcmp (header_register, "00COTAHIST", 10)) == 0) {

		*answer = BOVESPA_FILE_TYPE_HIST;
		SUCCESS;

	}
	else if ((memcmp (header_register, "00BDIN9999", 10)) == 0) {

		*answer = BOVESPA_FILE_TYPE_BDIN;
		SUCCESS;

	}
	else {

		CRIT ("unknown bovespa file type.");
		FAILURE;
	}

}


int discover_register_type (unsigned int file_type, const char *bovespa_register, unsigned int *answer) {

	/* 
	 * Match the type code field of the Bovespa register to a type code.
	 */

#define COMPARE_WITH(STRING) memcmp (bovespa_register, STRING, 2)

	switch (file_type) {

		case BOVESPA_FILE_TYPE_HIST:

			if ((COMPARE_WITH ("00")) == 0) {

				*answer = BOVESPA_FILE_SECTION_HEADER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("01")) == 0) {

				*answer = BOVESPA_FILE_SECTION_QUOTES;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("99")) == 0) {

				*answer = BOVESPA_FILE_SECTION_TRAILER;
				SUCCESS;

			}
			else {

				ERR ("unknown bovespa type field value '%c%c' for file type '%u'.", bovespa_register[0], bovespa_register[1], file_type);
				FAILURE;

			}
			break;

		case BOVESPA_FILE_TYPE_BDIN:

			if ((COMPARE_WITH ("00")) == 0) {

				*answer = BOVESPA_FILE_SECTION_HEADER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("01")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("02")) == 0) {

				*answer = BOVESPA_FILE_SECTION_QUOTES;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("03")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("04")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("05")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("06")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("07")) == 0) {

				*answer = BOVESPA_FILE_SECTION_OTHER;
				SUCCESS;

			}
			else if ((COMPARE_WITH ("99")) == 0) {

				*answer = BOVESPA_FILE_SECTION_TRAILER;
				SUCCESS;

			}
			else {

				ERR ("unknown bovespa type field value '%c%c' for file type '%u'.", bovespa_register[0], bovespa_register[1], file_type);
				FAILURE;

			}
			break;

		default:

			CRIT ("unknown bovespa file type '%u'.", file_type);
			FAILURE;

	}

#undef COMPARE_WITH

}

#undef FAILURE
#undef SUCCESS


void sanitize_field (char *field, size_t field_size) {

	size_t i;

	/* 
	 * Remove trailing spaces.
	 */

	for ( i = field_size - 1; i > 0; i-- ) {

		if (field[i] != ' ') {

			break;

		}
		field[i] = 0;
		field_size = i;

	}

	/* 
	 * Remove consecutive spaces.
	 */

	for ( i = 0; i < field_size; i++ ) {

		if (field[i] == ' ') {

			while (field[i + 1] == ' ') {

				memmove (&field[i], &field[i + 1], field_size - i);
				field_size--;

			}

		}

	}

	/* 
	 * Remove leading zeros.
	 */

	for ( i = 0; i < field_size; i++ ) {

		if (field[i] != '0') {

			break;

		}

	}
	memmove (field, &field[i], field_size - i + 1);

	/* 
	 * End of sanitization.
	 */

	return;

}


#define SUCCESS return (0)
#define IGNORE return (1)
#define FAILURE return (-1)

int quotes_list_append (const bovespa_mapper_t *mapper, quote_node_t **last) {

	char *aux_charp;	// General purpose short ranged character pointer.
	struct tm cal_time;	// Help during date/time type conversion.
	pfish_bovespa_daily_quote_t quote;	// Temporary quote structure for field type conversions.
	quote_node_t *new_node;		// New node to be added to the quotes linked list.

	/* 
	 * Consider only:
	 *
	 * 	- tp_merc = '010' (mercado a vista)
	 * 	- cod_bdi = '02' (lote padrão)
	 * 	- mod_ref = 'R$'
	 */

#define MATCH_AND_IGNORE(FIELD,STRING) \
	if ((strcmp (mapper->FIELD, STRING)) != 0) { \
		DEBUG ("register ignored due to field " #FIELD " ('%s') not be '%s'.", mapper->FIELD, STRING); \
		IGNORE; \
	}

	MATCH_AND_IGNORE (tp_merc, "10");
	MATCH_AND_IGNORE (cod_bdi, "2");
	MATCH_AND_IGNORE (mod_ref, "R$");

#undef MATCH_AND_IGNORE

	/*
	 * End of ignoring; starting field type conversions.
	 */

	/* 
	 * Convert field 'trading_date'.
	 */

#define STR2INT(TARGET,SOURCE) \
	cal_time.TARGET = strtol (mapper->SOURCE, &aux_charp, 10); \
	if ((*aux_charp) != 0) { \
		CRIT ("cannot understand bovespa field '" #SOURCE "' '%s' as a integer.", mapper->SOURCE); \
		FAILURE; \
	}

	cal_time.tm_sec = 0;
	cal_time.tm_min = 0;
	cal_time.tm_hour = 12;
	STR2INT (tm_mday, dia_pregao);
	STR2INT (tm_mon, mes_pregao);
	cal_time.tm_mon -= 1;
	STR2INT (tm_year, ano_pregao);
	cal_time.tm_year -= 1900;
	cal_time.tm_isdst = 0;
	cal_time.tm_gmtoff = 0;
	cal_time.tm_zone = NULL;

#undef STR2INT

	quote.trading_date = timegm (&cal_time);

	/*
	 * Convert field 'stock_spec'.
	 */

	memset (quote.stock_spec, 0, PFISH_BOVESPA_ESPECI_SIZE);
	strcpy (quote.stock_spec, mapper->especi);

	/*
	 * Convert unsigned integer fields.
	 */

#define STR2UINT(MAPPER_FIELD_NAME,QUOTE_FIELD_NAME) \
	quote.QUOTE_FIELD_NAME = strtoul (mapper->MAPPER_FIELD_NAME, &aux_charp, 10); \
	if (*aux_charp != 0) { \
		CRIT ("cannot understand bovespa field %s ('%s') as an unsigned integer.", #MAPPER_FIELD_NAME, mapper->MAPPER_FIELD_NAME); \
		FAILURE; \
	}

	STR2UINT (pre_abe, opening_price);
	STR2UINT (pre_max, maximum_price);
	STR2UINT (pre_min, minimum_price);
	STR2UINT (pre_med, average_price);
	STR2UINT (pre_ult, closing_price);
	STR2UINT (tot_neg, total_trades);
	STR2UINT (qua_tot, total_stocks);
	STR2UINT (vol_tot, total_volume);
	STR2UINT (fat_cot, price_factor);

#undef STR2UINT

	/*
	 * End of type conversions; add a new node to the end of the quotes linked list.
	 */

	if ((new_node = (quote_node_t *) malloc (sizeof (quote_node_t))) == NULL) {

		ALERT ("cannot allocate %u bytes of heap space.", sizeof (quote_node_t));
		FAILURE;

	}
	memset (new_node->stock.id, 0, PFISH_BOVESPA_CODNEG_SIZE);
	strcpy (new_node->stock.id, mapper->cod_neg);
	memcpy (&(new_node->quote), &quote, sizeof (pfish_bovespa_daily_quote_t));
	new_node->next = NULL;
	if (*last != NULL) {

		(*last)->next = new_node;

	}
	*last = new_node;
	
	/*
	 * All set.
	 */

	SUCCESS;

}

#undef FAILURE
#undef IGNORE
#undef SUCCESS


#define LESSER return (-1)
#define GREATER return (1)
#define EQUAL return (0)

int compare_quote_nodes (const void *a, const void *b) {

	int rcode;

#define CAST(X) (*((quote_node_t **) X))
#define A (CAST (a))
#define B (CAST (b))

	/* Sort by stock name ascending,
	 * then by timestamp ascending. */

	if ((rcode = strcmp (A->stock.id, B->stock.id)) < 0) {

		LESSER;

	}
	else if (rcode > 0) {

		GREATER;

	}
	else if (A->quote.trading_date < B->quote.trading_date) {

		LESSER;

	}
	else if (A->quote.trading_date > B->quote.trading_date) {

		GREATER;

	}
	else {

		EQUAL;

	}

#undef B
#undef A
#undef CAST

}

#undef EQUAL
#undef GREATER
#undef LESSER


#define SUCCESS return (0)
#define FAILURE return (-1)

int merge_daily_quotes (pfish_bovespa_daily_quote_t **a, size_t a_size, pfish_bovespa_daily_quote_t **b, size_t b_size, pfish_bovespa_daily_quote_t ***answer, size_t *answer_size) {

	pfish_bovespa_daily_quote_t **c;	// 'c' is the merged array.

	size_t a_count;		// Indexer for array 'a'.
	size_t b_count;		// Indexer for array 'b'.
	size_t c_count;		// Indexer for array 'c'.

	/*
	 * Make room for the worst case.
	 */

	if ((c = (pfish_bovespa_daily_quote_t **) malloc ((a_size + b_size) * sizeof (pfish_bovespa_daily_quote_t *))) == NULL) {

		ALERT ("cannot allocate %u bytes of heap space.", (a_size + b_size) * sizeof (pfish_bovespa_daily_quote_t *));
		FAILURE;

	}

#undef FAILURE
#define FAILURE \
	free (c); \
	return (-1)

	/*
	 * Merge arrays 'a' and 'b' into 'c'.
	 * Assumptions:
	 * 	- trading date is unique among elements of 'a'. Idem for 'b'. Idem for 'c'.
	 * 	- 'a' is sorted by trading date. Idem for 'b'. Idem for 'c'.
	 * 	- in case of elements in 'a' and 'b' with same trading date, element of 'b' takes precedence.
	 */

	a_count = 0;
	b_count = 0;
	c_count = 0;
	while ((a_count < a_size) || (b_count < b_size)) {

		/*
		 * There is at least one element to be merged from one of the input arrays.
		 */

		if (a_count >= a_size) {

			/*
			 * No elements left in 'a'.
			 */

			c[c_count++] = b[b_count++];

		}
		else if (b_count >= b_size) {

			/*
			 * No elements left in 'b'.
			 */

			c[c_count++] = a[a_count++];

		}
		else {

			/*
			 * Elements waiting in both input arrays; apply precedence rule.
			 */

#define TRADING_DATE(X) ((X[X##_count])->trading_date)

			if (TRADING_DATE (a) < TRADING_DATE (b)) {

				/*
				 * Trading of element in 'a' happened sooner.
				 */

				c[c_count++] = a[a_count++];

			}
			else if (TRADING_DATE (a) > TRADING_DATE (b)) {

				/*
				 * Trading of element in 'b' happened sooner.
				 */

				c[c_count++] = b[b_count++];

			}
			else {

				/*
				 * Same trading date for both elements.
				 * Element in 'b' wins, element in 'a' is discarded.
				 */

				c[c_count++] = b[b_count++];
				a_count++;

			}

#undef TRADING_DATE

		}

	}

	/*
	 * Merged array 'c' is mounted, containing 'c_count' elements.
	 */

	assert (c_count <= (a_count + b_count));
	*answer = c;
	*answer_size = c_count;
	SUCCESS;

}

#undef FAILURE
#undef SUCCESS

