package = "xtf8"
version = "master-1"

description = {
    summary = "Encode hybrid UTF-8 text and binary bytes to valid UTF-8 strings",
    homepage = "https://github.com/liweitianux/xtf8",
    license = "MIT",
}

dependencies = {}

source = {
    url = "git://github.com/liweitianux/xtf8",
    branch = "master",
}

local _defines = { "NDEBUG" }

build = {
    type = "builtin",
    modules = {
        ["xtf8.ffi"] = "xtf8.lua",
        ["libxtf8"] = {
            sources = { "xtf8.c" },
            defines = _defines,
        },
        ["xtf8"] = {
            sources = { "xtf8_lua.c", "xtf8.c" },
            defines = _defines,
        },
    },
}
