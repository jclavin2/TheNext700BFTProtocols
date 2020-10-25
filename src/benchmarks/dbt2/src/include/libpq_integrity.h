/*
 * odbc_stock_level.h
 *
 * This file is released under the terms of the Artistic License.  Please see
 * the file LICENSE, included in this package, for details.
 *
 * Copyright (C) 2002 Mark Wong & Open Source Development Lab, Inc.
 *
 * 23 july 2002
 * Based on TPC-C Standard Specification Revision 5.0.
 */

#ifndef _LIBPQ_INTEGRITY_H_
#define _LIBPQ_INTEGRITY_H_

#include <libpq_common.h>
#include <transaction_data.h>

int execute_integrity(struct db_context_t *dbc, struct integrity_t *data);

#endif /* _LIBPQ_INTEGRITY_H_ */
