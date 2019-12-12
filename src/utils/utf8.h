/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * Function: u8_char_len
 * Return the size of a given utf-8 character.
 */
int u8_char_len(const char *c);

/*
 * Function: u8_char_code
 * Return the unicode point for a given utf-8 char.
 */
int u8_char_code(const char *c);

/*
 * Function: u8_lower
 * Make an utf-8 string lowercase.
 */
void u8_lower(char *dst, const char *str, int n);

/*
 * Function: u8_upper
 * Make an utf-8 string uppercase.
 */
void u8_upper(char *dst, const char *str, int n);

/*
 * Function: u8_remove_accents
 * Replace accents to non accentuated letters in a utf8 string
 *
 * dst and str can be the same.
 */
void u8_remove_accents(char *dst, const char *str, int n);

/*
 * Function: u8_char_len
 * Return the number of characters in a utf-8 string.
 */
int u8_len(const char *str);
