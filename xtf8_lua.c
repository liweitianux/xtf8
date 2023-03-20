/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022-2023 Aaron LI
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

/*
 * API
 * ---
 * encoded = xtf8.encode(data, err?)
 * decoded = xtf8.decode(data, err?)
 *
 * The 'err' parameter is optional, and can have the following values:
 * - xtf8.ERR_REPLACE : replace conflicting characters (default)
 * - xtf8.ERR_ABORT : terminate the encoding process
 *
 * Usage
 * -----
 * local xtf8 = require("xtf8")
 * local encoded = xtf8.encode(data)
 * local decoded = xtf8.decode(encoded)
 * assert(decoded == data)
 */

#include <stdbool.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#include "xtf8.h"

/* NOTE: luaL_newlib() is available in Lua >=5.2 or LuaJIT >=2.1 */
#ifndef luaL_newlib
#define luaL_newlib(L, l) \
        (lua_newtable(L), luaL_register(L, NULL, l))
#endif


static int
l_helper(lua_State *L, bool is_encode)
{
    luaL_Buffer b;
    const char *in;
    char *p;
    size_t inlen, outlen;
    int err;
    uintptr_t (*f_xtf8)(void *, const void *, size_t, int);

    f_xtf8 = is_encode ? xtf8_encode : xtf8_decode;
    in = luaL_checklstring(L, 1, &inlen);
    err = luaL_optinteger(L, 2, XTF8_ERR_REPLACE);
    luaL_buffinit(L, &b);

    outlen = (size_t)f_xtf8(NULL, in, inlen, err);
    if ((uintptr_t)outlen == XTF8_ABORTED)
        return luaL_error(L, "found invalid sequence");

    if (outlen <= LUAL_BUFFERSIZE) {
        p = luaL_prepbuffer(&b);
        (void)f_xtf8(p, in, inlen, err);
        luaL_addsize(&b, outlen);

    } else {
        p = malloc(outlen);
        if (p == NULL)
            return luaL_error(L, "out of memory");

        (void)f_xtf8(p, in, inlen, err);
        luaL_addlstring(&b, p, outlen);
        free(p);
    }

    luaL_pushresult(&b);

    return 1;
}


static int
l_encode(lua_State *L)
{
    return l_helper(L, true);
}


static int
l_decode(lua_State *L)
{
    return l_helper(L, false);
}


int
luaopen_xtf8(lua_State *L)
{
    static const struct luaL_Reg funcs[] = {
        { "encode", l_encode },
        { "decode", l_decode },
        { NULL, NULL },
    };
    luaL_newlib(L, funcs);

    /* Error handlers */
    lua_pushinteger(L, XTF8_ERR_REPLACE);
    lua_setfield(L, -2, "ERR_REPLACE");
    lua_pushinteger(L, XTF8_ERR_ABORT);
    lua_setfield(L, -2, "ERR_ABORT");

    return 1;
}
