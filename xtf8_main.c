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
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xtf8.h"


#ifdef DEBUG
#define DPRINTF(fmt, ...)   \
        fprintf(stderr, "[DEBUG] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)   /* nothing */
#endif


/*
 * Dump the given $data of length $len in the same format as
 * 'hexdump -C'.
 */
static void
hexdump(FILE *fp, void *data, size_t len)
{
    static char hex[16 * 3 + 2]; /* extra space between octets */
    static char text[16 + 1];
    char *h, *t;
    uint8_t *p;
    size_t i, off;

    p = data;
    /* Suppress compilation warnings: -Wmaybe-uninitialized */
    h = hex;
    t = text;

    for (off = 0, i = 0; i < len; i++) {
        if (i % 16 == 0) {
            memset(hex, 0, sizeof(hex));
            memset(text, 0, sizeof(text));
            h = hex;
            t = text;
        }

        h += sprintf(h, "%02x %s", p[i], (i % 16 == 7) ? " " : "");
        t += sprintf(t, "%c", isprint(p[i]) ? p[i] : '.');

        if (i % 16 == 15 || i + 1 == len) {
            fprintf(fp, "%08zx  %-49.49s |%s|\n", off, hex, text);
            off = i + 1;
        }
    }

    fprintf(fp, "%08zx\n", len);
    fflush(fp);
}


/*
 * Escape the given UTF-8 string to be a valid JSON string.
 * (RFC 8259, Section 7)
 *
 * Credit: Nginx: src/core/ngx_string.c: ngx_escape_json()
 */
static uintptr_t
json_escape(void *dst, void *src, size_t len)
{
    uint8_t ch;
    uint8_t *d, *s, *end;
    size_t sz;

    d = dst;
    s = src;
    end = (uint8_t *)src + len;
    sz = 0;

    if (d == NULL) {
        /* determine the escaped string length */

        while (s < end) {
            ch = *s++;

            if (ch <= 0x1F) {
                /* escape control characters */
                sz++;
                switch (ch) {
                case '\n':
                case '\r':
                case '\t':
                case '\b':
                case '\f':
                    sz++;
                    break;
                default:
                    sz += sizeof("\\u00XX") - 2;
                }

            } else {
                if (ch == '\\' || ch == '"') {
                    /* escape reverse solidus (\) and quotation mark (") */
                    sz++;
                }
                sz++;
            }
        }

        return (uintptr_t)sz;
    }

    while (s < end) {
        ch = *s++;

        if (ch <= 0x1F) {
            *d++ = '\\';

            switch (ch) {
            case '\n':
                *d++ = 'n';
                break;
            case '\r':
                *d++ = 'r';
                break;
            case '\t':
                *d++ = 't';
                break;
            case '\b':
                *d++ = 'b';
                break;
            case '\f':
                *d++ = 'f';
                break;

            default: /* u00XX */
                *d++ = 'u';
                *d++ = '0';
                *d++ = '0';
                *d++ = '0' + (ch >> 4);
                ch &= 0xF;
                *d++ = (ch < 10) ? ('0' + ch) : ('A' + ch - 10);
            }

        } else {
            if (ch == '\\' || ch == '"')
                *d++ = '\\';
            *d++ = ch;
        }
    }

    return (uintptr_t)d;
}


#define JSON_ERR_UNESCAPE   ((uintptr_t)-1)

/*
 * Unescape the given JSON string to its original UTF-8 string.
 */
static uintptr_t
json_unescape(void *dst, void *src, size_t len)
{
    uint8_t *d, *s, *end;
    size_t sz;
    bool escape;

    d = dst;
    end = (uint8_t *)src + len;
    sz = 0;
    escape = false;

    for (s = src; s < end; s++) {
        if (escape) {
            switch (*s) {
            case '\\':
                sz++;
                if (d != NULL)
                    *d++ = '\\';
                break;
            case '"':
                sz++;
                if (d != NULL)
                    *d++ = '"';
                break;

            case 'n':
                sz++;
                if (d != NULL)
                    *d++ = '\n';
                break;
            case 'r':
                sz++;
                if (d != NULL)
                    *d++ = '\r';
                break;
            case 't':
                sz++;
                if (d != NULL)
                    *d++ = '\t';
                break;
            case 'b':
                sz++;
                if (d != NULL)
                    *d++ = '\b';
                break;
            case 'f':
                sz++;
                if (d != NULL)
                    *d++ = '\f';
                break;

            case 'u': /* u00XX */
            {
                uint32_t codepoint;
                int i, x, ch;

                if (s + 5 >= end) {
                    DPRINTF("truncated \\u00XX sequence");
                    return JSON_ERR_UNESCAPE;
                }
                for (codepoint = 0, i = 1; i < 5; i++) {
                    ch = *(s + i);
                    if (ch >= '0' && ch <= '9') {
                        x = ch - '0';
                    } else if (ch >= 'A' && ch <= 'F') {
                        x = ch - 'A' + 10;
                    } else {
                        DPRINTF("invalid xdigit in \\u00XX sequence: %.*s", 5, s);
                        return JSON_ERR_UNESCAPE;
                    }
                    codepoint = (codepoint << 4) | x;
                }
                if (codepoint > 0x1F) {
                    DPRINTF("out-of-range \\u00XX sequence: %.*s", 5, s);
                    return JSON_ERR_UNESCAPE;
                }
                DPRINTF("unescaped \\%.*s to 0x%02X", 5, s, codepoint);
                s += 4;

                sz++;
                if (d != NULL)
                    *d++ = (uint8_t)codepoint;

                break;
            }

            default:
                DPRINTF("invalid escape sequence");
                return JSON_ERR_UNESCAPE;
            }

            escape = false;

        } else if (*s == '\\') {
            escape = true;

        } else {
            sz++;
            if (d != NULL)
                *d++ = *s;
        }
    }

    if (escape) {
        DPRINTF("incomplete escape sequence");
        return JSON_ERR_UNESCAPE;
    }

    return (d != NULL) ? (uintptr_t)d : (uintptr_t)sz;
}


/*
 * Read from the given file $fp until EOF, and return the data,
 * with data length save in $size.
 *
 * The returned data must be free()'d after use.
 */
static void *
read_file(FILE *fp, size_t *size)
{
    static size_t block_size = 1024;
    unsigned char *buf, *tmp;
    size_t n, sz, buf_size, tmp_size;

    buf = tmp = NULL;
    buf_size = sz = 0;

    do {
        if (sz + block_size > buf_size) {
            tmp_size = (buf_size == 0 ? block_size : buf_size * 2);
            tmp = calloc(1, tmp_size);
            if (tmp == NULL) {
                fprintf(stderr, "%s: calloc() failed: %s\n",
                        __func__, strerror(errno));
                goto err;
            }

            if (buf != NULL) {
                memcpy(tmp, buf, buf_size);
                free(buf);
            }

            buf = tmp;
            buf_size = tmp_size;
            DPRINTF("allocated buffer of size %zu", buf_size);
        }

        n = fread(buf + sz, 1, block_size, fp);
        sz += n;
        DPRINTF("read %zu bytes", n);

        if (n != block_size) {
            if (feof(fp)) {
                break;
            } else {
                fprintf(stderr, "%s: fread() failed: %s\n",
                        __func__, strerror(errno));
                goto err;
            }
        };

    } while (!feof(fp));

    *size = sz;
    return buf;

err:
    if (buf)
        free(buf);
    return NULL;
}


/*
 * Write the given $data of length $len to file $fp.
 */
static int
write_file(FILE *fp, void *data, size_t len)
{
    static size_t block_size = 1024;
    unsigned char *d = data;
    size_t n, nw, sz;

    n = 0;
    while (len > 0) {
        sz = (len > block_size) ? block_size : len;
        nw = fwrite(d + n, 1, sz, fp);
        if (nw != sz) {
            fprintf(stderr, "%s: fwrite() failed: %s\n",
                    __func__, strerror(errno));
            return -1;
        }
        n += nw;
        len -= nw;
    }

    fflush(fp);
    return 0;
}


__attribute__((noreturn))
static void
usage(void)
{
    fputs("XTF8 codec utility\n"
          "\n"
          "usage: xtf8 [OPTIONS]\n"
          "\n"
          "options:\n"
          "    -d : decode mode instead of encode\n"
          "    -i <infile> : input file (stdin if unspecified)\n"
          "    -o <outfile> : output file (stdout if unspecified)\n"
          "    -j : JSON escape the output (encode mode) or unescape the input (decode mode)\n"
          "    -x : hexdump the output\n"
          "    -D : show verbose debug messages\n"
          "\n",
          stderr);

    exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
    const char *infile, *outfile;
    void *input, *output;
    size_t inlen, outlen;
    FILE *infp, *outfp;
    bool debug, decode, escape, hex;
    int xtf8_err, opt;
    uintptr_t (*f_xtf8)(void *, void *, size_t, int);

    infile = outfile = NULL;
    infp = outfp = NULL;
    debug = decode = escape = hex = false;
    input = output = NULL;
    xtf8_err = XTF8_ERR_REPLACE;
    f_xtf8 = xtf8_encode;

    while ((opt = getopt(argc, argv, "Ddhi:jo:x")) != -1) {
        switch (opt) {
        case 'D':
            debug = true;
            break;
        case 'd':
            decode = true;
            f_xtf8 = xtf8_decode;
            break;
        case 'i':
            infile = optarg;
            break;
        case 'j':
            escape = true;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'x':
            hex = true;
            break;
        case 'h':
        default:
            usage();
            /* NOTREACHED */
        }
    }

    argc -= optind;
    if (argc != 0) {
        fprintf(stderr, "ERROR: received extra arguments.\n");
        usage();
        /* NOTREACHED */
    }

    if (debug) {
        fprintf(stderr, "Mode: %s\n", decode ? "decode" : "encode");
        fprintf(stderr, "Input: %s\n", infile ? infile : "<stdin>");
        fprintf(stderr, "Output: %s\n", outfile ? outfile : "<stdout>");
        fprintf(stderr, "JSON: %s\n",
                escape ?
                (decode ? "unescape input" : "escape output") :
                "(none)");
    }

    if (infile != NULL) {
        infp = fopen(infile, "r");
        if (infp == NULL)
            err(1, "fopen(%s)", infile);
    }
    if (outfile != NULL) {
        outfp = fopen(outfile, "w");
        if (outfp == NULL)
            err(1, "fopen(%s)", outfile);
    }

    input = read_file((infp ? infp : stdin), &inlen);
    if (input == NULL || inlen == 0)
        errx(1, "failed to read from: %s", infp ? infile : "stdin");

    if (debug) {
        fprintf(stderr, "Input: (len=%zu)\n", inlen);
        hexdump(stderr, input, inlen);
    }

    if (escape && decode) {
        /* JSON unescape input. */
        void *jbuf;
        size_t jlen;

        jlen = (size_t)json_unescape(NULL, input, inlen);
        if ((uintptr_t)jlen == JSON_ERR_UNESCAPE)
            errx(1, "failed to unescape JSON string");

        jbuf = calloc(1, jlen);
        if (jbuf == NULL)
            err(1, "failed to allocate JSON buffer");

        (void)json_unescape(jbuf, input, inlen);

        free(input);
        input = jbuf;
        inlen = jlen;

        if (debug) {
            fprintf(stderr, "JSON-unescaped input: (len=%zu)\n", inlen);
            hexdump(stderr, input, inlen);
        }
    }

    outlen = (size_t)f_xtf8(NULL, input, inlen, xtf8_err);
    assert((uintptr_t)outlen != XTF8_ABORTED);
    if (debug) {
        fprintf(stderr, "XTF8 %s size: %zu -> %zu\n",
                (decode ? "decoded" : "encoded"),
                inlen, outlen);
    }

    output = calloc(1, outlen);
    if (output == NULL)
        err(1, "failed to allocate output buffer");

    (void)f_xtf8(output, input, inlen, xtf8_err);

    if (debug) {
        fprintf(stderr, "Output: (len=%zu)\n", outlen);
        hexdump(stderr, output, outlen);
    }

    if (escape && !decode) {
        /* JSON escape encoded output. */
        void *jbuf;
        size_t jlen;

        jlen = (size_t)json_escape(NULL, output, outlen);
        jbuf = calloc(1, jlen);
        if (jbuf == NULL)
            err(1, "failed to allocate JSON buffer");

        (void)json_escape(jbuf, output, outlen);

        free(output);
        output = jbuf;
        outlen = jlen;

        if (debug) {
            fprintf(stderr, "JSON-escaped output: (len=%zu)\n", outlen);
            hexdump(stderr, output, outlen);
        }
    }

    if (hex)
        hexdump(stdout, output, outlen);
    else
        write_file((outfile ? outfp : stdout), output, outlen);

    free(input);
    free(output);

    if (infp != NULL)
        fclose(infp);
    if (outfp != NULL)
        fclose(outfp);

    return 0;
}
