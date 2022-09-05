/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "utf8.h"
#include <string.h>
#include <assert.h>

static const char LEN_TABLE[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * List of accentued characters:
 *   Accent Upper       2 bytes.
 *   Accent Lower       2 bytes.
 *   Non accent Upper   1 byte.
 *   Non accent Lower   1 byte.
 *   Padding            2 bytes.
 */
static const char *ACCENTS =
    "ĀāAa  ÁáAa  ǍǎAa  ÀàAa  ÂâAa  ÄäAa  ÃãAa  "
    "ĒēEe  ÉéEe  ĚěEe  ÈèEe  ÊêEe  ËëEe  "
    "ĪīIi  ÍíIi  ǏǐIi  ÌìIi  ÎîIi  ÏïIi  "
    "ŌōOo  ÓóOo  ǑǒOo  ÒòOo  ÔôOo  ÖöOo  ÕõOo  "
    "ŪūUu  ÚúUu  ǓǔUu  ÙùUu  ÛûUu  ÜüUu  ǕǖUu  ǗǘUu  ǙǚUu  ǛǜUu  "
    "ÑñNn  "
    "ÇçCc  ";

int u8_char_len(const char *c)
{
    return LEN_TABLE[(unsigned int)(unsigned char)c[0]] + 1;
}

void u8_lower(char *dst, const char *str, int size)
{
    const char* ptr;
    int len;
    while (*str) {
        len = u8_char_len(str);
        if (len + 1 > size) break;
        if (len == 1 && *str >= 'A' && *str <= 'Z') {
            *dst = *str - 'A' + 'a';
        } else if (len == 2) {
            memcpy(dst, str, 2);
            for (ptr = ACCENTS; *ptr; ptr += 8) {
                if (memcmp(ptr, str, 2) == 0) {
                    memcpy(dst, ptr + 2, 2);
                    break;
                }
            }
        } else {
            memcpy(dst, str, len);
        }
        str += len;
        dst += len;
        size -= len;
    }
    *dst = '\0';
}

void u8_upper(char *dst, const char *str, int size)
{
    const char* ptr;
    int len;
    while (*str) { //  && n > 2) {
        len = u8_char_len(str);
        if (len + 1 > size) break;
        if (len == 1 && *str >= 'a' && *str <= 'z') {
            *dst = *str - 'a' + 'A';
        } else if (len == 2) {
            memcpy(dst, str, 2);
            for (ptr = ACCENTS; *ptr; ptr += 8) {
                if (memcmp(ptr + 2, str, 2) == 0) {
                    memcpy(dst, ptr, 2);
                    break;
                }
            }
        } else {
            memcpy(dst, str, len);
        }
        str += len;
        dst += len;
        size -= len;
    }
    *dst = '\0';
}

int u8_len(const char *str)
{
    int len = 0;
    while (*str) {
        str += u8_char_len(str);
        len++;
    }
    return len;
}

int u8_char_code(const char *c)
{
    const unsigned char *s = (const unsigned char*)c;
    switch (u8_char_len(c)) {
    case 1:
        return s[0];
    case 2:
        return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    case 3:
        return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
    default:
        return ' ';
    }
}

/*
 * Function: u8_remove_accents
 * Replace accents to non accentuated letters in a utf8 string
 *
 * dst and str can be the same.
 */
void u8_remove_accents(char *dst, const char *str, int n)
{
    int len;
    const char *ptr;
    while (*str && n > 0) {
        len = u8_char_len(str);
        memcpy(dst, str, len);
        str += len;
        if (len == 2) {
            for (ptr = ACCENTS; *ptr; ptr += 8) {
                if (memcmp(ptr + 0, dst, 2) == 0) {
                    len = 1;
                    *dst = *(ptr + 4);
                    break;
                }
                if (memcmp(ptr + 2, dst, 2) == 0) {
                    len = 1;
                    *dst = *(ptr + 5);
                    break;
                }
            }
        }
        dst += len;
        n -= len;
    }
    *dst = '\0';
}


int u8_split_line(char *dst, int len, const char *src, int min_chars)
{
    int n = 1, word_len, max_len = 0;
    char *p, *next;
    if (dst != src) snprintf(dst, len, "%s", src);
    for (p = dst; *p; p += u8_char_len(p), n++) {
        if (*p != ' ') continue;
        next = strchr(p + 1, ' ');
        word_len = next ? next - p + 1 : u8_len(p + 1);
        if (n + word_len > min_chars) {
            if (n - 1 > max_len) max_len = n - 1;
            n = 0;
            *p = '\n';
        }
    }
    if (n - 1 > max_len) max_len = n - 1;
    return max_len;
}


/******** TESTS ***********************************************************/

#if COMPILE_TESTS

#include "tests.h"

static void test_u8_split_line(void)
{
    char* s = "coucou coucou cou";
    char buf[128];
    assert(u8_split_line(buf, sizeof(buf), s, 8) == 6);
    assert(u8_split_line(buf, sizeof(buf), s, 30) == 17);

    s = "cc coucoucou c";
    assert(u8_split_line(buf, sizeof(buf), s, 8) == 9);

    s = "这是一份非 常间单的说明书";
    assert(u8_split_line(buf, sizeof(buf), s, 8) == 7);
    assert(u8_split_line(buf, sizeof(buf), s, 30) == 13);
    assert(u8_split_line(buf, sizeof(buf), s, 13) == 13);
    s = "这是 一份 明书";
    assert(u8_split_line(buf, sizeof(buf), s, 5) == 5);
}

TEST_REGISTER(NULL, test_u8_split_line, TEST_AUTO);

#endif
