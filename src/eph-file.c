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

static file_t g_file = {};

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

#define WRITE(file, v, type) ({ \
        type v_ = (type)v; \
        CHECK(fwrite((char*)&(v_), sizeof(v_), 1, file) == 1); \
    })

#define READ(data, size, type) ({ \
        type v_; \
        CHECK(size >= sizeof(type)); \
        memcpy(&v_, data, sizeof(type)); \
        size -= sizeof(type); \
        data += sizeof(type); \
        v_; \
    })

static void chunk_write_start(chunk_t *c, const char *type)
{
    memset(c, 0, sizeof(*c));
    memcpy(c->type, type, 4);
    c->buffer = calloc(1, CHUNK_BUFF_SIZE);
}

static void chunk_write(chunk_t *c, const char *data, int size)
{
    assert(c->length + size < CHUNK_BUFF_SIZE);
    memcpy(c->buffer + c->length, data, size);
    c->length += size;
}

static void chunk_write_finish(chunk_t *c, FILE *out)
{
    fwrite(c->type, 4, 1, out);
    WRITE(out, c->length, int32_t);
    fwrite(c->buffer, c->length, 1, out);
    WRITE(out, 0, int32_t);    // CRC XXX: todo.
    free(c->buffer);
    c->buffer = NULL;
}

#define CHUNK_WRITE(chunk, v, type) ({ \
        type v_ = (type)v; chunk_write(chunk, (char*)&(v_), sizeof(v_)); })

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

#define CHUNK_WRITE(chunk, v, type) ({ \
        type v_ = (type)v; chunk_write(chunk, (char*)&(v_), sizeof(v_)); })

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

void eph_file_start(int nb_per_tile)
{
    assert(!g_file.entries);
    g_file.nb_per_tile = nb_per_tile;
}

int eph_file_add(const char type[4], double vmag, const double pos[3],
                  int version, const void *data, int size, const char *id)
{
    entry_t *e;
    assert(id);
    assert(size <= ARRAY_SIZE(e->data));

    HASH_FIND_STR(g_file.entries, id, e);
    if (e) return 1; // Duplicate.

    e = calloc(1, sizeof(*e));

    e->vmag = vmag;
    vec3_copy(pos, e->pos);
    strcpy(e->id, id);
    memcpy(e->data, data, size);
    HASH_ADD_STR(g_file.entries, id, e);

    g_file.data_size = g_file.data_size ?: size;
    g_file.version = g_file.version ?: version;
    memcpy(g_file.type, type, 4);
    assert(g_file.data_size == size);
    assert(g_file.version == version);
    return 0;
}

static int striped_len(const char *str)
{
    int ret;
    ret = strlen(str);
    while (ret && str[ret - 1] == ' ') ret--;
    return ret;
}

void eph_file_add_name(const char *id, const char *name)
{

    chunk_t *c = &g_file.names_chunk;
    if (!c->buffer) chunk_write_start(c, "NAME");
    CHUNK_WRITE(c, strlen(id), int32_t);
    chunk_write(c, id, strlen(id));
    CHUNK_WRITE(c, striped_len(name), int32_t);
    chunk_write(c, name, striped_len(name));
}

static int sort_cmp(const void *_a, const void *_b)
{
    const entry_t *a, *b;
    a = _a;
    b = _b;
    return cmp(a->vmag, b->vmag);
}

static const char *get_tile_path(const char *base, int order, int pix)
{
    static char path[1024]; // XXX: no overflow check.
    int dir = (pix / 10000) * 10000;
    // Since mkdir doesn't work with deep path, we have to do it step by
    // step.
    mkdir(base, 0777);
    sprintf(path, "%s/Norder%d", base, order);
    mkdir(path, 0777);
    sprintf(path, "%s/Norder%d/Dir%d", base, order, dir);
    mkdir(path, 0777);
    sprintf(path, "%s/Norder%d/Dir%d/Npix%d.eph", base, order, dir, pix);
    return path;
}

void eph_file_save(const char *path)
{
    entry_t *e, *etmp;
    tile_t *tile, *tmp;
    int data_size, order, pix;
    double theta, phi;
    uint64_t nuniq;
    bool singlefile;
    const char *tilepath;
    uint8_t *buf;
    unsigned long comp_size;
    char namespath[1024];
    FILE *file = NULL;
    chunk_t c;

    singlefile = str_endswith(path, ".eph");
    assert(g_file.entries);
    data_size = g_file.data_size;
    HASH_SORT(g_file.entries, sort_cmp);
    // For each entry, try to add to the lowest tile that is not already full.
    for (e = g_file.entries; e != NULL; e = e->hh.next) {
        for (order = 0; ;order++) {
            // grid_get_tile(pos.level, e->pos, &pos.x, &pos.y);
            eraC2s(e->pos, &theta, &phi);
            healpix_ang2pix(1 << order, theta, phi, &pix);
            nuniq = pix + 4 * (1L << (2 * order));
            HASH_FIND(hh, g_file.tiles, &nuniq, sizeof(nuniq), tile);
            if (!tile) {
                tile = calloc(1, sizeof(*tile));
                tile->nuniq = nuniq;
                tile->data = calloc(g_file.nb_per_tile, data_size);
                HASH_ADD(hh, g_file.tiles, nuniq, sizeof(nuniq), tile);
            }
            if (tile->nb < g_file.nb_per_tile) {
                memcpy(tile->data + tile->nb * data_size, e->data, data_size);
                tile->nb++;
                break;
            } // Otherwise, try higher levels.
        }
    }

    // Save the file to disk.
    if (singlefile) {
        file = fopen(path, "wb");
        fwrite("EPHE", 4, 1, file);
        WRITE(file, FILE_VERSION, int32_t);
    }
    HASH_ITER(hh, g_file.tiles, tile, tmp) {
        if (!singlefile) {
            order = log2(tile->nuniq / 4) / 2;
            pix = tile->nuniq - 4 * (1L << (2 * order));
            assert(pix >= 0);
            tilepath = get_tile_path(path, order, pix);
            file = fopen(tilepath, "wb");
            fwrite("EPHE", 4, 1, file);
            WRITE(file, FILE_VERSION, int32_t);
        }

        comp_size = compressBound(tile->nb * g_file.data_size);
        buf = calloc(1, comp_size);
        compress(buf, &comp_size, tile->data, g_file.data_size * tile->nb);

        chunk_write_start(&c, g_file.type);
        CHUNK_WRITE(&c, g_file.version, int32_t);
        CHUNK_WRITE(&c, tile->nuniq, uint64_t);
        CHUNK_WRITE(&c, tile->nb * g_file.data_size, int32_t);
        CHUNK_WRITE(&c, comp_size, int32_t);
        chunk_write(&c, (const char*)buf, comp_size);

        free(buf);

        chunk_write_finish(&c, file);
        HASH_DELETE(hh, g_file.tiles, tile);
        free(tile->data);
        free(tile);

        if (!singlefile) fclose(file);
    }
    if (g_file.names_chunk.length) {
        if (!singlefile) {
            sprintf(namespath, "%s/names.eph", path);
            file = fopen(namespath, "wb");
            fwrite("EPHE", 4, 1, file);
            WRITE(file, FILE_VERSION, int32_t);
        }
        chunk_write_finish(&g_file.names_chunk, file);
        if (!singlefile) fclose(file);
    }

    if (singlefile)
        fclose(file);

    HASH_ITER(hh, g_file.entries, e, etmp) {
        HASH_DELETE(hh, g_file.entries, e);
        free(e);
    }

    memset(&g_file, 0, sizeof(g_file));
}
