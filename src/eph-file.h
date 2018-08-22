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

void eph_file_register_tile_type(const char type[4],
        int (*f)(int version, int order, int pix,
                  int size, void *data, void *user));

int eph_load(const void *data, int data_size, void *user);

#endif // EPH_FILE_H
