/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022 Aaron LI
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "utf8.h" /* utf8_decode() */
#include "xtf8.h"


#ifdef  XTF8_DEBUG
#include <stdio.h>
#define DPRINTF(fmt, ...)   \
        fprintf(stderr, "[DEBUG] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)   /* nothing */
#endif


/*
 * CSUR registered Private User Area (PUA) reserved for encoding hacks.
 * https://www.evertype.com/standards/csur/conscript-table.html
 *
 * Registered by MirBSD for the OPTU encoding:
 * https://www.mirbsd.org/htman/i386/man3/optu8to16.htm
 */
#define XTF8_PUA_START  0xEF80
#define XTF8_PUA_END    0xEFFF


#ifndef NDEBUG

/*
 * Verify whether the given $data of length $len is valid UTF-8 sequence.
 */
static bool
is_utf8(void *data, size_t len)
{
    unsigned char *p, *end;
    uint32_t state, codepoint;

    p = data;
    end = p + len;
    state = UTF8_ACCEPT;

    while (p < end) {
        utf8_decode(&state, &codepoint, *p);
        p++;
    }

    return state == UTF8_ACCEPT;
}

#endif


uintptr_t
xtf8_encode(void *dst, void *src, size_t len, int error)
{
    uint32_t s_prev, s_cur, codepoint;
    uint8_t *d, *s, *pos, *end;
    size_t sz;

    s_prev = s_cur = UTF8_ACCEPT;
    d = dst;
    pos = s = src;
    end = (uint8_t *)src + len;
    sz = 0;

    while (s < end) {
        switch (utf8_decode(&s_cur, &codepoint, *s)) {

        case UTF8_ACCEPT:
            if (codepoint >= XTF8_PUA_START && codepoint <= XTF8_PUA_END) {
                /* Found a collision! */
                if (error == XTF8_ERR_ABORT)
                    return (uintptr_t)-1;

                /* Replace with the Unicode Replacement Character. */
                codepoint = 0xFFFD; /* UTF-8: <EF BF BD> */
                DPRINTF("Replaced -> U+%04X", codepoint);
                sz += 3;
                if (d != NULL) {
                    *d++ = (codepoint >> 12 & 0x0F) | 0xE0;
                    *d++ = (codepoint >> 6  & 0x3F) | 0x80;
                    *d++ = (codepoint       & 0x3F) | 0x80;
                }

            } else {
                /* Valid UTF-8 sequence, copy it. */
                DPRINTF("U+%04X", codepoint);
                sz += 1 + s - pos;
                if (d != NULL) {
                    while (pos <= s)
                        *d++ = *pos++;
                }
            }

            pos = s + 1;
            break;

        case UTF8_REJECT:
            /* Invalid UTF-8 sequence; encode to PUA. */
            s_cur = UTF8_ACCEPT;
            if (s_prev != UTF8_ACCEPT)
                s--; /* Retry with this byte as the beginning. */

            /*
             * Every invalid byte is transliterated to a Unicode character
             * of range U+EF80..U+EFFF within the Private User Area (PUA),
             * which is encoded to 3 bytes in UTF-8 sequence.
             */
            while (pos <= s) {
                assert(*pos >= 0x80); /* Must be non-ASCII characters. */
                sz += 3;
                if (d != NULL) {
                    codepoint = 0xEF80 | (*pos & 0x7F);
                    DPRINTF("Encoded 0x%02x -> U+%04X", *pos, codepoint);
                    *d++ = (codepoint >> 12 & 0x0F) | 0xE0;
                    *d++ = (codepoint >> 6  & 0x3F) | 0x80;
                    *d++ = (codepoint       & 0x3F) | 0x80;
                }
                pos++;
            }

            /* 'pos' already equals to 's + 1' */
            break;

        default:
            /* More bytes to read. */
            break;
        }

        s_prev = s_cur;
        s++;
    }

    if (d != NULL) {
        assert((uint8_t *)dst + sz == d);
        assert(is_utf8(dst, sz));
        return (uintptr_t)d;
    } else {
        return (uintptr_t)sz;
    }
}


uintptr_t
xtf8_decode(void *dst, void *src, size_t len, int error)
{
    uint32_t s_prev, s_cur, codepoint;
    uint8_t v, *d, *s, *pos, *end;
    size_t sz;

    s_prev = s_cur = UTF8_ACCEPT;
    d = dst;
    pos = s = src;
    end = (uint8_t *)src + len;
    sz = 0;

    while (s < end) {
        switch (utf8_decode(&s_cur, &codepoint, *s)) {

        case UTF8_ACCEPT:
            if (codepoint >= XTF8_PUA_START && codepoint <= XTF8_PUA_END) {
                /*
                 * Found an encoded value; decode it.
                 *
                 * WARNING: Must decode the value to non-ASCII character
                 *          (i.e., 0x80..0xFF) to avoid security issues!
                 */
                v = (codepoint & 0x7F) | 0x80;
                assert(v >= 0x80);
                DPRINTF("Decoded U+%04X -> 0x%02x", codepoint, v);
                sz += 1;
                if (d != NULL) {
                    *d++ = v;
                }

            } else {
                /* Valid UTF-8 sequence, copy it. */
                DPRINTF("U+%04X", codepoint);
                sz += 1 + s - pos;
                if (d != NULL) {
                    while (pos <= s)
                        *d++ = *pos++;
                }
            }

            pos = s + 1;
            break;

        case UTF8_REJECT:
            /* Invalid UTF-8 sequence! */
            if (error == XTF8_ERR_ABORT)
                return (uintptr_t)-1;

            s_cur = UTF8_ACCEPT;
            if (s_prev != UTF8_ACCEPT)
                s--; /* Retry with this byte as the beginning. */

            /* Replace with the Unicode Replacement Character. */
            codepoint = 0xFFFD; /* UTF-8: <EF BF BD> */
            DPRINTF("Replaced -> U+%04X", codepoint);
            sz += 3;
            if (d != NULL) {
                *d++ = (codepoint >> 12 & 0x0F) | 0xE0;
                *d++ = (codepoint >> 6  & 0x3F) | 0x80;
                *d++ = (codepoint       & 0x3F) | 0x80;
            }

            pos = s + 1;
            break;

        default:
            /* More bytes to read. */
            break;
        }

        s_prev = s_cur;
        s++;
    }

    if (d != NULL) {
        assert((uint8_t *)dst + sz == d);
        return (uintptr_t)d;
    } else {
        return (uintptr_t)sz;
    }
}
