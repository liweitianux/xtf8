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

#ifndef XTF8_H_
#define XTF8_H_

#include <stddef.h> /* size_t */
#include <stdint.h> /* uintptr_t */


/* Error handlers */
enum {
    XTF8_ERR_REPLACE, /* replace conflicting characters */
    XTF8_ERR_ABORT, /* terminate the encoding process */
};

/* Error return codes */
#define XTF8_ABORTED    ((uintptr_t)-1)


/*
 * Encode the given data in $src of length $len, place the result in
 * $dst, and return a pointer to the end of the used $dst buffer.
 *
 * The required output buffer size can be obtained by calling with
 * $dst = NULL.
 *
 * If error occurred, then return a value of (uintptr_t)-1.
 *
 * The $error parameter specifies the error handling method:
 * - XTF8_ERR_REPLACE: replace conflicting PUA characters with U+FFFD
 * - XTF8_ERR_ABORT: abort the encoding if found any conflicts
 */
uintptr_t xtf8_encode(void *dst, const void *src, size_t len, int error);

/*
 * Decode the given data in $src of length $len, place the result in
 * $dst, and return a pointer to the end of the used $dst buffer.
 *
 * The required output buffer size can be obtained by calling with
 * $dst = NULL, but it's easier to just allocate a buffer of the same
 * size as the input.
 *
 * If error occurred, then return a value of (uintptr_t)-1.
 *
 * The $error parameter specifies the error handling method:
 * - XTF8_ERR_REPLACE: replace invalid characters with U+FFFD
 * - XTF8_ERR_ABORT: abort the encoding if found any invalid sequences
 */
uintptr_t xtf8_decode(void *dst, const void *src, size_t len, int error);


#endif
