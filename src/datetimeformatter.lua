
local liblibc = require 'liblibc'
local lambda = require 'operator'
local libdatetimeformatter = require 'libdatetimeformatter'

local FIELD_MAP = {}

FIELD_MAP['G'] = ChronoField.ERA	                       -- SDF, LDML (different to both for 1/2 chars)
FIELD_MAP['y'] = ChronoField.YEAR_OF_ERA	               -- SDF, LDML
FIELD_MAP['u'] = ChronoField.YEAR	                      -- LDML (different in SDF)
FIELD_MAP['Q'] = IsoFields.QUARTER_OF_YEAR	             -- LDML (removed quarter from 310)
FIELD_MAP['q'] = IsoFields.QUARTER_OF_YEAR	             -- LDML (stand-alone)
FIELD_MAP['M'] = ChronoField.MONTH_OF_YEAR	             -- SDF, LDML
FIELD_MAP['L'] = ChronoField.MONTH_OF_YEAR	             -- SDF, LDML (stand-alone)
FIELD_MAP['D'] = ChronoField.DAY_OF_YEAR	               -- SDF, LDML
FIELD_MAP['d'] = ChronoField.DAY_OF_MONTH	              -- SDF, LDML
FIELD_MAP['F'] = ChronoField.ALIGNED_DAY_OF_WEEK_IN_MONTH	  -- SDF, LDML
FIELD_MAP['E'] = ChronoField.DAY_OF_WEEK	               -- SDF, LDML (different to both for 1/2 chars)
FIELD_MAP['c'] = ChronoField.DAY_OF_WEEK	               -- LDML (stand-alone)
FIELD_MAP['e'] = ChronoField.DAY_OF_WEEK	               -- LDML (needs localized week number)
FIELD_MAP['a'] = ChronoField.AMPM_OF_DAY	               -- SDF, LDML
FIELD_MAP['H'] = ChronoField.HOUR_OF_DAY	               -- SDF, LDML
FIELD_MAP['k'] = ChronoField.CLOCK_HOUR_OF_DAY	         -- SDF, LDML
FIELD_MAP['K'] = ChronoField.HOUR_OF_AMPM	              -- SDF, LDML
FIELD_MAP['h'] = ChronoField.CLOCK_HOUR_OF_AMPM	        -- SDF, LDML
FIELD_MAP['m'] = ChronoField.MINUTE_OF_HOUR	            -- SDF, LDML
FIELD_MAP['s'] = ChronoField.SECOND_OF_MINUTE	          -- SDF, LDML
FIELD_MAP['S'] = ChronoField.NANO_OF_SECOND	            -- LDML (SDF uses milli-of-second number)
FIELD_MAP['A'] = ChronoField.MILLI_OF_DAY	              -- LDML
FIELD_MAP['n'] = ChronoField.NANO_OF_SECOND	            -- 310 (proposed for LDML)
FIELD_MAP['N'] = ChronoField.NANO_OF_DAY	               -- 310 (proposed for LDML)
FIELD_MAP['g'] = JulianFields.MODIFIED_JULIAN_DAY

local datetimeformatter = {}	-- our final module.

datetimeformatter.FIELD_MAP = FIELD_MAP

function datetimeformatter.ofPattern (pattern)

	-- simply forward the call providing the map of fields.
	return libdatetimeformatter.ofPattern (pattern, string.gsub)

end

return datetimeformatter
