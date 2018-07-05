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
 * NAME chunck: a list of (id, name) tuples.
 *  For each entry:
 *      4 bytes: id size
 *      n bytes: id
 *      4 bytes: name size
 *      n bytes: name
 *
 * We can register tile chunks with the eph_file_register_tile_type function.
 * Tiles chunks all have the following structure:
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
    int      length;
    uint32_t crc;
    char     *buffer;   // Used when writing.

    int      pos;
} chunk_t;

static struct {
    int (*callback)(int version, int order, int pix,
                    int size, void *data, void *user);
    char type[4];
} g_tile_types[8] = {};

// Structs used when we prepare a file for saving.
typedef struct {
    UT_hash_handle  hh;
    char    id[16];  // uniq key in the hash.
    double  vmag;
    double  pos[3];
    char    data[64];
} entry_t;

typedef struct {
    UT_hash_handle  hh;
    uint64_t        nuniq; // Hips nuniq position.
    int             nb;
    void            *data;
} tile_t;

typedef struct {
    char        type[4];
    int         version;
    int         nb_per_tile;
    int         data_size;
    entry_t     *entries; // Hash of id -> entry
    tile_t      *tiles;
    chunk_t     names_chunk;
} file_t;

void eph_file_register_tile_type(const char type[4],
        int (*f)(int version, int order, int pix,
                  int size, void *data, void *user))

{
    int i;
    for (i = 0; i < ARRAY_SIZE(g_tile_types); i++) {
        if (!g_tile_types[i].callback) break;
    }
    assert(i < ARRAY_SIZE(g_tile_types));
    g_tile_types[i].callback = f;
    memcpy(g_tile_types[i].type, type, 4);
}

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

static void load_names_chunk(chunk_t *c, const void **data, int *data_size)
{
    int len;
    char id[64], name[128];
    while (c->pos < c->length) {
        len = CHUNK_READ(c, data, data_size, int32_t);
        assert(len < ARRAY_SIZE(id));
        chunk_read(c, data, data_size, id, len);
        id[len] = '\0';
        len = CHUNK_READ(c, data, data_size, int32_t);
        assert(len < ARRAY_SIZE(name));
        chunk_read(c, data, data_size, name, len);
        name[len] = '\0';
        identifiers_add(id, "NAME", name, NULL, NULL);
    }
    chunk_read_finish(c, data, data_size);
}

int eph_load_file(const char *path, void *user)
{
    int size, ret;
    void *data = read_file(path, &size);
    ret = eph_load(data, size, user);
    free(data);
    return ret;
}

int eph_load(const void *data, int data_size, void *user)
{
    chunk_t c;
    int version, tile_version;
    int i, order, pix, comp_size;
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
        if (strncmp(c.type, "NAME", 4) == 0) {
            load_names_chunk(&c, &data, &data_size);
            continue;
        }
        for (i = 0; i < ARRAY_SIZE(g_tile_types); i++) {
            if (strncmp(g_tile_types[i].type, c.type, 4) == 0) break;
        }
        if (i == ARRAY_SIZE(g_tile_types)) {
            LOG_E("Unknown chunk type: %.4s", c.type);
            assert(false);
        }

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
        chunk_read_finish(&c, &data, &data_size);
        g_tile_types[i].callback(tile_version, order, pix,
                                 size, chunk_data, user);
        free(chunk_data);
    }
    return 0;
}
