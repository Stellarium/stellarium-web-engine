/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef EPH_FILE_H
#define EPH_FILE_H

#include <stdint.h>

#include "json.h"

/*
 * File: eph-file.h
 * Some support functions to read and write eph file format.
 *
 * See eph-file.c for some doc about the format.
 */


int eph_load(const void *data, int data_size, void *user,
             int (*callback)(const char type[4],
                             const void *data, int size,
                             const json_value *json,
                             void *user));

int eph_read_tile_header(const void *data, int data_size, int *data_ofs,
                         int *version, int *order, int *pix);

void *eph_read_compressed_block(const void *data, int data_size,
                                int *data_ofs, int *size);

void eph_shuffle_bytes(uint8_t *data, int nb, int size);

/*
 * Enum: EPH_UNIT
 * Represent the different unit we can use for eph file data.
 *
 * Warning: don't change those values since they are also set in the data
 * files!
 *
 * The value has two 16 bits parts: the least significant 16 bits are a
 * bitfield of factor to multiply in order to convert between related units.
 * This allow to eventually change the units in the data file without having to
 * update the code as long as the conversion flags are correct.
 */
enum {
    EPH_RAD             = 1 << 16,
    EPH_DEG             = EPH_RAD | 1,      // Degree
    EPH_ARCMIN          = EPH_DEG | 2,      // (1/60)
    EPH_ARCSEC          = EPH_ARCMIN | 4,   // (1/60)

    EPH_VMAG            = 3 << 16,

    EPH_RAD_PER_YEAR    = 6 << 16,
    EPH_YEAR            = 7 << 16,
    EPH_KM_PER_SEC      = 8 << 16,

    // Legacy unit still used in gaia survey.
    EPH_ARCSEC_         = 5 << 16 | 1 | 2 | 4,
};

typedef struct eph_table_column {
    char        name[4];
    char        type;
    int         unit;

    // Attributes filled by eph_read_table_prepare.
    int         got;    // Set if present in the source file.
    int         start;
    int         size;
    int         src_unit;
    int         row_size;
} eph_table_column_t;

int eph_read_table_header(int version, const void *data, int data_size,
                          int *data_ofs, int *row_size, int *flags,
                          int nb_columns, eph_table_column_t *columns);

int eph_read_table_row(const void *data, int data_size, int *data_ofs,
                       int nb_columns, const eph_table_column_t *columns,
                       ...);

#endif // EPH_FILE_H
