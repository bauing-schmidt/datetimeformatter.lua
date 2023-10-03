
local dtf = require 'datetimeformatter'

local compiled_pattern, standalone = dtf.compile "yyyy.MM.dd G 'at' HH:mm:ss z"

print (compiled_pattern)

print (dtf.tm_t_2_timer {

    hour = 12,
    isdst = false,
    mday = 0,
    min = 08,
    mon = 7,
    sec = 56,
    wday = 0,
    yday = 0,
    year = 2001,
    gmtoff = -700,
    zone = "PDT",
})

for k, v in pairs (dtf.timer_2_tm_t (60952043336, false)) do print (k, v) end

print ()

for k, v in pairs (dtf.timer_2_tm_t (os.time(), false)) do print (k, v) end

--[[
local formatted = dtf.format {

    pattern = compiled_pattern,
    date = os.time (),

    isWeekDateSupported = false,
    getWeekYear = 'unsupported'

    isGregorianCalendar = true,

    -- the following keys have to have integer values.
    [dtf.calendar_fields.ERA] = '',
    [dtf.calendar_fields.YEAR] = '',
    [dtf.calendar_fields.MONTH] = '',
    [dtf.calendar_fields.DATE] = '',
    [dtf.calendar_fields.HOUR_OF_DAY] = '',
    [dtf.calendar_fields.MINUTE] = '',
    [dtf.calendar_fields.SECOND] = '',
    [dtf.calendar_fields.MILLISECOND] = '',
    [dtf.calendar_fields.DAY_OF_WEEK] = '',
    [dtf.calendar_fields.DAY_OF_YEAR] = '',
    [dtf.calendar_fields.DAY_OF_WEEK_IN_MONTH] = '',
    [dtf.calendar_fields.WEEK_OF_YEAR] = '',
    [dtf.calendar_fields.WEEK_OF_MONTH] = '',
    [dtf.calendar_fields.AM_PM] = '',
    [dtf.calendar_fields.HOUR] = '',
    [dtf.calendar_fields.ZONE_OFFSET] = '',
    [dtf.calendar_fields.WEEK_YEAR] = '',
    [dtf.calendar_fields.ISO_DAY_OF_WEEK] = '',

    -- the following keys are locale-dependent.
    getEras = [ '', '' ],
    getMonths = [],
    getShortMonths = [],
    getWeekdays = [],
    getShortWeekdays = [],
    getAmPmStrings = [],
    
    getTimeZone = [],
    getShortTimeZone = [],

}

--]]