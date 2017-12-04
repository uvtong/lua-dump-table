local dump = require "dump"
local util = require "util"

local fd = io.open("Item.lua")
local str = fd:read("*a")

local tbl = load(str)()


local str = dump.pack(tbl)

print(str)
local t = dump.unpack(str)

util.dump_table(t)