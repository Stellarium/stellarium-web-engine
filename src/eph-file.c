/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "eph-file.h"
#include "swe.h"
#include "zlib.h"

#include <assert.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>

/* The stars tile file format is as follow:
 *
 * 4 bytes magic string:    "EPHE"
 * 4 bytes file version:    <FILE_VERSION>
 * List of chuncks
 *
 * chunk:
 *  4 bytes: type
 *  4 bytes: data len
 *  4 bytes: data
 *  4 bytes: CRC
 *
 * If type starts with an uppercase letter, this means we got an healpix
 * tile chunck, with the following structure:
 *
 *  4 bytes: version
 *  8 bytes: nuniq hips tile pos
 *  4 bytes: data size
 *  4 bytes: compressed data size
 *  n bytes: compressed data
 *
 */

#define FILE_VERSION 2

typedef struct {
    char     type[4];
    char     type_padding_; // Ensure that type is null terminated.
    int      length;
    uint32_t crc;
    char     *buffer;   // Used when writing.

    int      pos;
} chunk_t;


#define CHUNK_BUFF_SIZE (1 << 20) // 1 MiB max buffer size!

#define READ(data, size, type) ({ \
        type v_; \
        CHECK(size >= sizeof(type)); \
        memcpy(&v_, data, sizeof(type)); \
        size -= sizeof(type); \
        data += sizeof(type); \
        v_; \
    })

static bool chunk_read_start(chunk_t *c, const void **data, int *data_size)
{
    memset(c, 0, sizeof(*c));
    if (*data_size == 0) return false;
    CHECK(*data_size >= 8);
    memcpy(c->type, *data, 4);
    *data += 4; *data_size -= 4;
    c->length = READ(*data, *data_size, int32_t);
    return true;
}

static void chunk_read_finish(chunk_t *c, const void **data, int *data_size)
{
    int crc;
    assert(c->pos == c->length);
    crc = READ(*data, *data_size, int32_t);
    (void)crc; // TODO: check crc.
}

static void chunk_read(chunk_t *c, const void **data, int *data_size,
                       char *buff, int size)
{
    c->pos += size;
    assert(c->pos <= c->length);
    CHECK(*data_size >= size);
    memcpy(buff, *data, size);
    *data += size;
    *data_size -= size;
}

#define CHUNK_READ(chunk, data, data_size, type) ({ \
        type v_; \
        chunk_read(chunk, data, data_size, (char*)&(v_), sizeof(v_)); \
        v_; \
    })

int eph_load(const void *data, int data_size, void *user,
             int (*callback)(const char type[4], int version,
                             int order, int pix,
                             int size, void *data, void *user))
{
    chunk_t c;
    int version, tile_version;
    int order, pix, comp_size;
    uint64_t nuniq;
    unsigned long size;
    void *chunk_data, *buf;

    assert(data);
    CHECK(data_size >= 4);
    CHECK(strncmp(data, "EPHE", 4) == 0);
    data += 4; data_size -= 4;
    version = READ(data, data_size, int32_t);
    CHECK(version == FILE_VERSION);
    while (chunk_read_start(&c, &data, &data_size)) {
        // Uppercase starting chunks are healpix tiles.
        if (c.type[0] >= 'A' && c.type[0] <= 'Z') {
            tile_version = CHUNK_READ(&c, &data, &data_size, int32_t);
            nuniq = CHUNK_READ(&c, &data, &data_size, uint64_t);
            order = log2(nuniq / 4) / 2;
            pix = nuniq - 4 * (1 << (2 * order));
            size = CHUNK_READ(&c, &data, &data_size, int32_t);
            comp_size = CHUNK_READ(&c, &data, &data_size, int32_t);
            chunk_data = malloc(size);
            buf = malloc(comp_size);
            chunk_read(&c, &data, &data_size, buf, comp_size);
            uncompress(chunk_data, &size, buf, comp_size);
            free(buf);
            callback(c.type, tile_version, order, pix, size, chunk_data, user);
            free(chunk_data);
        }
        chunk_read_finish(&c, &data, &data_size);
    }
    return 0;
}

// In place shuffle of the data bytes for optimized compression.
static void shuffle_bytes(uint8_t *data, int nb, int size)
{
    int i, j;
    uint8_t *buf = calloc(nb, size);
    memcpy(buf, data, nb * size);
    for (j = 0; j < size; j++) {
        for (i = 0; i < nb; i++) {
            data[j * nb + i] = buf[i * size + j];
        }
    }
    free(buf);
}

int eph_read_table_prepare(int version, void *data, int data_size,
                           int *data_ofs, int row_size,
                           int nb_columns, eph_table_column_t *columns)
{
    int i, start = 0;

    assert(*data_ofs == 0);
    if (version > 0)
        shuffle_bytes(data, row_size, data_size / row_size);
    for (i = 0; i < nb_columns; i++) {
        columns[i].start = start;
        columns[i].src_unit = columns[i].unit;
        if (!columns[i].size) {
            switch (columns[i].type) {
            case 'i':
            case 'f': columns[i].size = 4; break;
            case 'Q': columns[i].size = 8; break;
            }
        }
        start += columns[i].size;
    }
    return 0;
}

double eph_convert_f(int src_unit, int unit, double v)
{
    assert(src_unit == unit);
    return v;
}

int eph_read_table_row(const void *data, int data_size, int *data_ofs,
                       int row_size, int nb_columns,
                       const eph_table_column_t *columns,
                       ...)
{
    int i;
    va_list ap;
    union {
        char    *s;
        int      i;
        float    f;
        uint64_t q;
    } v;

    va_start(ap, columns);
    for (i = 0; i < nb_columns; i++) {
        switch (columns[i].type) {
        case 'i':
            memcpy(&v.i, data + *data_ofs + columns[i].start, 4);
            *va_arg(ap, int*) = v.i;
            break;
        case 'f':
            memcpy(&v.f, data + *data_ofs + columns[i].start, 4);
            *va_arg(ap, double*) = eph_convert_f(
                    columns[i].src_unit, columns[i].unit, v.f);
            break;
        case 'Q':
            memcpy(&v.q, data + *data_ofs + columns[i].start, 8);
            *va_arg(ap, uint64_t*) = v.q;
            break;
        case 's':
            memcpy(va_arg(ap, char*), data + *data_ofs + columns[i].start,
                   columns[i].size);
            break;
        }
    }
    va_end(ap);
    *data_ofs += row_size;
    return 0;
}
