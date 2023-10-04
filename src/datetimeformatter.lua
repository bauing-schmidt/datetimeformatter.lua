

local lambda = require 'operator'
local libdatetimeformatter = require 'libdatetimeformatter'

local memo_patterns = {}

local dtf = {}

setmetatable (dtf, {
    __index = libdatetimeformatter,
})

function dtf.compile (pattern)
    local C = memo_patterns[pattern]
    if not C then
        C = libdatetimeformatter.compile (pattern)
        memo_patterns[pattern] = C
    end
    return C
end

return dtf