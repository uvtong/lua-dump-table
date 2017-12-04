local dump = require "dump"
local util = require "util"

local mrq = {
	girl = "hx",
	age = 29,
	hx = {
		[1] = "1",
		["live"] = "th",
		age = 24
	},
	haha = {
		{1,2,3},
		{"a" , b = {a = 2}},
		a = "mrq",
		{2,1},
	}
}

local str = dump.pack_sort(mrq)

print(str)
local t = dump.unpack(str)

util.dump_table(t)