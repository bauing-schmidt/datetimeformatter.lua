local op = require 'operator'
local dtf = require 'datetimeformatter'



local t = os.time()

for i = 1, 1000000 do dtf.format (dtf.compile "yyyy MMM", dtf.time(), 'en_GB.utf8', 2000, 'GMT', true) end

print (os.time() - t)
