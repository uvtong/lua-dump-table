local dump = require "dump"
local util = require "util"

 local Ab = {
	 	person = {
	 		{
	 			name = "Alice",
	 			id = 10000,
	 			phone = {
	 				{number = "123456789",type = 1},
	 				{number = "87654321",type = 2},
	 			}
	 		},
	 		{
	 			name = "Bob",
	 			id = 20000,
	 			phone = {
	 				{number = "01234567890",type = 3},
	 			}
	 		},
	 	}
	 }

local str = dump.pack_sort(Ab)

local now = os.time()
local t
for i = 1,1 do
	t = dump.unpack(str)
end
util.dump_table(t)
print(os.time()-now)

-- local now = os.time()
-- str = "return"..str
-- for i = 1,1024*1024 do
-- 	local t = load(str)()
-- end

-- print(os.time()-now)