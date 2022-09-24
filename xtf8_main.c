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
    buf_size = tmp_size = sz = 0;

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
          "    -i : input file (stdin if unspecified)\n"
          "    -o : output file (stdout if unspecified)\n"
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
    unsigned char *input, *output;
    size_t inlen, outlen;
    FILE *infp, *outfp;
    bool debug, decode, hex;
    int xtf8_err, opt;
    uintptr_t (*f_xtf8)(void *, void *, size_t, int);

    infile = outfile = NULL;
    infp = outfp = NULL;
    debug = decode = hex = false;
    input = output = NULL;
    inlen = outlen = 0;
    xtf8_err = XTF8_ERR_REPLACE;
    f_xtf8 = xtf8_encode;

    while ((opt = getopt(argc, argv, "Ddhi:o:x")) != -1) {
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

    if (debug && !hex) {
        fprintf(stderr, "Output: (len=%zu)\n", outlen);
        hexdump(stderr, output, outlen);
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
