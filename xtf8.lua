--[[
-- SPDX-License-Identifier: MIT
--
-- Copyright (c) 2022 Aaron LI
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files (the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject
-- to the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
-- IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
-- CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
-- TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
--]]

--[[
XTF8 LuaJIT FFI interface.

API
---
encoded = xtf8.encode(data, err?)
decoded = xtf8.decode(data, err?)

The 'err' parameter is optional, and can have the following values:
- xtf8.ERR_REPLACE : replace conflicting characters (default)
- xtf8.ERR_ABORT : terminate the encoding process

Usage
-----
local xtf8 = require("xtf8")
local encoded = xtf8.encode(data)
local decoded = xtf8.decode(encoded)
assert(decoded == data)
--]]

local ffi = require("ffi")
local C = ffi.C


ffi.cdef[[
enum {
    XTF8_ERR_REPLACE, /* replace conflicting characters */
    XTF8_ERR_ABORT, /* terminate the encoding process */
};

/*
 * NOTE: Cannot convert to 'static const', because it only work for
 *       integer types up to 32 bits.
 * See: https://luajit.org/ext_ffi_semantics.html#status
 */
// #define XTF8_ABORTED    (uintptr_t)-1;

uintptr_t xtf8_encode(void *dst, const void *src, size_t len, int error);
uintptr_t xtf8_decode(void *dst, const void *src, size_t len, int error);
]]

-- NOTE: Cannot just use 'ffi.cast("uintptr_t", -1)' because it is undefined.
-- See: https://github.com/LuaJIT/LuaJIT/issues/459
local xtf8_aborted = ffi.cast("uintptr_t", ffi.typeof("int")(-1))


local get_buffer
do
    local _buf_type = ffi.typeof("unsigned char[?]")
    local _buf_size = 4096
    local _buf

    function get_buffer(size)
        if size > _buf_size then
            return ffi.new(_buf_type, size)
        end
        if not _buf then
            _buf = ffi.new(_buf_type, _buf_size)
        end
        return _buf
    end
end


local function xtf8_encode(data, err)
    err = err or C.XTF8_ERR_REPLACE
    local len = C.xtf8_encode(nil, data, #data, err)
    if len == xtf8_aborted then
        return nil, "found invalid sequence"
    end

    local buf = get_buffer(len)
    C.xtf8_encode(buf, data, #data, err)
    return ffi.string(buf, len)
end


local function xtf8_decode(data, err)
    err = err or C.XTF8_ERR_REPLACE
    local len = C.xtf8_decode(nil, data, #data, err)
    if len == xtf8_aborted then
        return nil, "found invalid sequence"
    end

    local buf = get_buffer(len)
    C.xtf8_decode(buf, data, #data, err)
    return ffi.string(buf, len)
end


local _M = {
    ERR_REPLACE = C.XTF8_ERR_REPLACE,
    ERR_ABORT   = C.XTF8_ERR_ABORT,

    encode = xtf8_encode,
    decode = xtf8_decode,
}


return _M
