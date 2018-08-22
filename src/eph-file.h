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
             int (*callback)(const char type[4], int version,
                             int order, int pix,
                             int size, void *data, void *user));

typedef struct eph_table_column {
    char        name[4];
    char        type;
    int         unit;

    // Attributes filled by eph_read_table_prepare.
    int         start;
    int         size;
    int         src_unit;
} eph_table_column_t;

int eph_read_table_prepare(int version, void *data, int data_size,
                           int *data_ofs, int row_size,
                           int nb_columns, eph_table_column_t *columns);

int eph_read_table_row(const void *data, int data_size, int *data_ofs,
                       int row_size, int nb_columns,
                       const eph_table_column_t *columns,
                       ...);

#endif // EPH_FILE_H
