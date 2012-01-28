/*
 * pilot_fish/bovespa.h
 * pilot_fish Bovespa daily quotes manipulation.
 */

#ifndef FILE_PFISH_BOVESPA_SEEN
#define FILE_PFISH_BOVESPA_SEEN

#include <stddef.h>
#include <time.h>

#include <pilot_fish/bovespa_stdint.h>


/*
 * Library info structure.
 */

struct pfish_bovespa_library_info {

	char *build_date;		// Date when the library was built.
	char *build_time;		// Time of day when the library was built.
	char *compiler_version;		// Version of the compiled used to build the library.

};

typedef struct pfish_bovespa_library_info pfish_bovespa_library_info_t;


/*
 * Library info getter.
 * Fills an info structure with static information about the library.
 *
 * param[out] target pointer to a info structure to be filled.
 */

void pfish_bovespa_library_info_get (pfish_bovespa_library_info_t *target);


/*
 * Sizes of text fields of Bovespa stocks.
 * Sizes are designed to always hold the string terminator.
 */

#define PFISH_BOVESPA_CODNEG_SIZE 13
#define PFISH_BOVESPA_ESPECI_SIZE 11


/*
 * Bovespa stock id type.
 * This type uniquely identifies a stock.
 */

struct pfish_bovespa_stock_id {

	char id[PFISH_BOVESPA_CODNEG_SIZE];	// Identification of a stock.

};

typedef struct pfish_bovespa_stock_id pfish_bovespa_stock_id_t;


/*
 * Bovespa stock list structure.
 */

struct pfish_bovespa_stock_list {

	size_t stock_list_size;	// How many elements in stock_list[].
	pfish_bovespa_stock_id_t stock_list[];	// Elements are ordered (id, ascending).

};

typedef struct pfish_bovespa_stock_list pfish_bovespa_stock_list_t;


/*
 * Bovespa stock list structure allocator.
 * 
 * @return dynamically allocated stock list structure on success, NULL on failure.
 */

pfish_bovespa_stock_list_t *pfish_bovespa_stock_list_alloc ();


/*
 * Bovespa quotes of one day of trading.
 */

struct pfish_bovespa_daily_quote {

	time_t trading_date;
	char stock_spec[PFISH_BOVESPA_ESPECI_SIZE];

	pfish_uint16_t price_factor;	// Unit stock price = price field value / price_factor.

	pfish_uint64_t opening_price;	// In units of 1/100 of the stock currency.
	pfish_uint64_t closing_price;	// In units of 1/100 of the stock currency.
	pfish_uint64_t minimum_price;	// In units of 1/100 of the stock currency.
	pfish_uint64_t maximum_price;	// In units of 1/100 of the stock currency.
	pfish_uint64_t average_price;	// In units of 1/100 of the stock currency.

	pfish_uint16_t total_trades;
	pfish_uint64_t total_stocks;
	pfish_uint64_t total_volume;	// In units of 1/100 of the stock currency.

};

typedef struct pfish_bovespa_daily_quote pfish_bovespa_daily_quote_t;


/*
 * Bovespa trading history of a stock.
 */

struct pfish_bovespa_stock_history {

	size_t daily_quotes_size;	// How many elements in daily_quotes[].
	size_t last_xplit;	// Index of daily_quotes[] of the most recent inplit or split, 0 if no inplit or split happened.
	pfish_bovespa_daily_quote_t daily_quotes[];	// Elements are ordered by trading date (ascending, unique).

};

typedef struct pfish_bovespa_stock_history pfish_bovespa_stock_history_t;


/*
 * Bovespa stock history structure allocator.
 * 
 * @param[in] stock_id stock identification.
 * @param[out] answer dynamically allocated stock history structure if stock exists in database, NULL otherwise.
 *
 * @return 0 on success, negative on failure.
 */

int pfish_bovespa_stock_history_alloc (const pfish_bovespa_stock_id_t *stock_id, pfish_bovespa_stock_history_t **answer);


/*
 * Bovespa stock history structure releaser.
 *
 * @param target stock history structure allocated with pfish_bovespa_stock_history_alloc().
 *
 * @return 0 on success, negative on failure.
 */

int pfish_bovespa_stock_history_free (pfish_bovespa_stock_history_t *target);


#endif	// FILE_PFISH_BOVESPA_SEEN

