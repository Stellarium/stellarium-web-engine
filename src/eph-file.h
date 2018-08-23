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

/*
 * File: eph-file.h
 * Some support functions to read and write eph file format.
 *
 * See eph-file.c for some doc about the format.
 */


int eph_load(const void *data, int data_size, void *user,
             int (*callback)(const char type[4],
                             const void *data, int size, void *user));

int eph_read_tile_header(const void *data, int data_size, int *data_ofs,
                         int *version, int *order, int *pix);

void *eph_read_compressed_block(const void *data, int data_size,
                                int *data_ofs, int *size);

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
    EPH_DEG             = 2 << 16 | 1,          // D2R
    EPH_VMAG            = 3 << 16,
    EPH_ARCMIN          = 4 << 16 | 1 | 2,      // D2R * (1/60)
    EPH_ARCSEC          = 5 << 16 | 1 | 2 | 4,  // D2R * (1/60) * (1/60)
    EPH_RAD_PER_YEAR    = 6 << 16,
    EPH_RAD_PER_DAY     = 7 << 16 | 8,          // 365.25
};

typedef struct eph_table_column {
    char        name[4];
    char        type;
    int         unit;

    // Attributes filled by eph_read_table_prepare.
    int         row_size;
    int         start;
    int         size;
    int         src_unit;
} eph_table_column_t;

int eph_read_table_prepare(int version, void *data, int data_size,
                           int *data_ofs, int row_size,
                           int nb_columns, eph_table_column_t *columns);

int eph_read_table_row(const void *data, int data_size, int *data_ofs,
                       int nb_columns, const eph_table_column_t *columns,
                       ...);

#endif // EPH_FILE_H
