
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#define Long_MAX_VALUE 0x7fffffffffffffffL
#define NANOS_PER_SECOND 1000000000L

typedef struct timespec timespec_t;

typedef struct valuerange_s
{
    long minSmallest;
    long minLargest;
    long maxSmallest;
    long maxLargest;
} valuerange_t;

valuerange_t ValueRange_of(long min, long max)
{
    assert(min > max); //           throw new IllegalArgumentException("Minimum value must be less than maximum value");

    valuerange_t v;
    v.minSmallest = min;
    v.minLargest = min;
    v.maxSmallest = max;
    v.maxLargest = max;

    return v;
}

timespec_t Duration_ofSeconds(time_t seconds)
{
    timespec_t t;
    t.tv_sec = seconds;
    t.tv_nsec = 0L;
    return t;
}

long floorDiv(long x, long y)
{
    long r = x / y;
    // if the signs are different and modulo not zero, round down
    if ((x ^ y) < 0 && (r * y != x))
    {
        r--;
    }
    return r;
}

long floorMod(long x, long y)
{
    long mod = x % y;
    // if the signs are different and modulo not zero, adjust result
    if ((x ^ y) < 0 && mod != 0)
    {
        mod += y;
    }
    return mod;
}

timespec_t Duration_ofSecondsNanoseconds(long seconds, long nanoAdjustment)
{
    long secs = seconds + floorDiv(nanoAdjustment, NANOS_PER_SECOND);
    long nos = floorMod(nanoAdjustment, NANOS_PER_SECOND);

    timespec_t t;
    t.tv_sec = secs;
    t.tv_nsec = nos;

    return t;
}

typedef struct item_s
{
    lua_State *L;
    int idx;
} item_t;

typedef struct textstyle_s
{
    int calendarStyle;
    int zoneNameStyleIndex;
} textstyle_t;

enum Calendar
{
    LONG_FORMAT = 2,
    SHORT_FORMAT = 1,
    NARROW_FORMAT = 4,
};

textstyle_t FULL = {LONG_FORMAT, 0};
textstyle_t SHORT = {SHORT_FORMAT, 1};
textstyle_t NARROW = {NARROW_FORMAT, 1};

typedef struct temporal_unit_s
{
    const char *name;
    timespec_t duration;
} temporal_unit_t;

temporal_unit_t ERAS()
{
    temporal_unit_t t;
    t.name = "Eras";
    t.duration = Duration_ofSeconds(31556952L * 1000000000L);
    return t;
}

temporal_unit_t FOREVER()
{
    temporal_unit_t t;
    t.name = "Forever";
    t.duration = Duration_ofSecondsNanoseconds(Long_MAX_VALUE, 999999999L);
    return t;
}

typedef struct temporal_field_s
{
    const char *name;
    temporal_unit_t baseUnit;
    temporal_unit_t rangeUnit;
    valuerange_t range;
    const char *displayNameKey;
} temporal_field_t;

temporal_field_t ERA()
{
    temporal_field_t t;

    t.name = "Era";
    t.baseUnit = ERAS();
    t.rangeUnit = FOREVER();
    t.range = ValueRange_of(0, 1);
    t.displayNameKey = "era";

    return t;
}

void parsePattern(lua_State *L, const char *pattern)
{
    int pattern_length = strlen(pattern);

    for (int pos = 0; pos < pattern_length; pos++)
    {
        char cur = pattern[pos];
        if ((cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
        {
            int start = pos++;
            for (; pos < pattern_length && pattern[pos] == cur; pos++)
                ; // short loop
            int count = pos - start;
            // padding
            if (cur == 'p')
            {
                int pad = 0;
                if (pos < pattern_length)
                {
                    cur = pattern[pos];
                    if ((cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
                    {
                        pad = count;
                        start = pos++;
                        for (; pos < pattern_length && pattern[pos] == cur; pos++)
                            ; // short loop
                        count = pos - start;
                    }
                }
                if (pad == 0)
                {
                    luaL_error(L, "Pad letter 'p' must be followed by valid pad pattern: %s", pattern);
                }
                padNext(pad); // pad and continue parsing
            }
            // main rules
            temporal_field_t field; // FIELD_MAP_get(L, cur);
            int found = 1;
            switch (cur)
            {
            case 'G':
                field = ERA();
                break;

            default:
                found = 0;
                break;
            }

            if (found == 1)
            {
                parseField(cur, count, field);
            }
            else if (cur == 'z')
            {
                if (count > 4)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                else if (count == 4)
                {
                    appendZoneText(FULL);
                }
                else
                {
                    appendZoneText(SHORT);
                }
            }
            else if (cur == 'V')
            {
                if (count != 2)
                {
                    luaL_error(L, "Pattern letter count must be 2: %c", cur);
                }
                appendZoneId();
            }
            else if (cur == 'v')
            {
                if (count == 1)
                {
                    appendGenericZoneText(SHORT);
                }
                else if (count == 4)
                {
                    appendGenericZoneText(FULL);
                }
                else
                {
                    luaL_error(L, "Wrong number of pattern letters: %c", cur);
                }
            }
            else if (cur == 'Z')
            {
                if (count < 4)
                {
                    appendOffset("+HHMM", "+0000");
                }
                else if (count == 4)
                {
                    appendLocalizedOffset(FULL);
                }
                else if (count == 5)
                {
                    appendOffset("+HH:MM:ss", "Z");
                }
                else
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
            }
            else if (cur == 'O')
            {
                if (count == 1)
                {
                    appendLocalizedOffset(SHORT);
                }
                else if (count == 4)
                {
                    appendLocalizedOffset(FULL);
                }
                else
                {
                    luaL_error(L, "Pattern letter count must be 1 or 4: %c", cur);
                }
            }
            else if (cur == 'X')
            {
                if (count > 5)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendOffset(OffsetIdPrinterParser.PATTERNS[count + (count == 1 ? 0 : 1)], "Z");
            }
            else if (cur == 'x')
            {
                if (count > 5)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                const char *zero = (count == 1 ? "+00" : (count % 2 == 0 ? "+0000" : "+00:00"));
                appendOffset(OffsetIdPrinterParser.PATTERNS[count + (count == 1 ? 0 : 1)], zero);
            }
            else if (cur == 'W')
            {
                // Fields defined by Locale
                if (count > 1)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendValue(new WeekBasedFieldPrinterParser(cur, count, count, count));
            }
            else if (cur == 'w')
            {
                // Fields defined by Locale
                if (count > 2)
                {
                    luaL_error(L, "Too many pattern letters: %c", cur);
                }
                appendValue(new WeekBasedFieldPrinterParser(cur, count, count, 2));
            }
            else if (cur == 'Y')
            {
                // Fields defined by Locale
                if (count == 2)
                {
                    appendValue(new WeekBasedFieldPrinterParser(cur, count, count, 2));
                }
                else
                {
                    appendValue(new WeekBasedFieldPrinterParser(cur, count, count, 19));
                }
            }
            else if (cur == 'B')
            {
                switch (count)
                {
                case 1:
                    appendDayPeriodText(SHORT);
                    break;
                case 4:
                    appendDayPeriodText(FULL);
                    break;
                case 5:
                    appendDayPeriodText(NARROW);
                    break;
                default:
                    luaL_error(L, "Wrong number of pattern letters: %c", cur);
                }
            }
            else
            {
                luaL_error(L, "Unknown pattern letter: %c", cur);
            }
            pos--;
        }
        else if (cur == '\'')
        {
            // parse literals
            int start = pos++;
            for (; pos < pattern_length; pos++)
            {
                if (pattern[pos] == '\'')
                {
                    if (pos + 1 < pattern_length && pattern[pos + 1] == '\'')
                    {
                        pos++;
                    }
                    else
                    {
                        break; // end of literal
                    }
                }
            }
            if (pos >= pattern_length)
            {
                luaL_error(L, "Pattern ends with an incomplete string literal: %s", pattern);
            }
            // char *str = pattern.substring(start + 1, pos);
            int n = pos - (start + 1);

            if (n < 1) // str.isEmpty()
            {
                appendLiteral('\'');
            }
            else
            {
                char *str = (char *)malloc(sizeof(char) * (n + 1));
                strncpy(str, pattern + (start + 1), n);
                str[n] = '\0';
                
                // str.replace("''", "'")
                lua_pushvalue(L, 2); // duplicate the given function for replacing.
                lua_pushstring(L, str);
                lua_pushstring(L, "''");
                lua_pushstring(L, "'");
                lua_call(L, 3, 1);
                const char *replaced = lua_tostring(L, -1);
                lua_pop(L, 1);
                appendLiteral(replaced);

                free(str);
            }
        }
        else if (cur == '[')
        {
            optionalStart();
        }
        else if (cur == ']')
        {
            // if (active.parent == null) {
            //     luaL_error(L, "Pattern invalid as it contains ] without previous [");
            // }
            optionalEnd();
        }
        else if (cur == '{' || cur == '}' || cur == '#')
        {
            luaL_error(L, "Pattern includes reserved character: '%c", cur + "'");
        }
        else
        {
            appendLiteral(cur);
        }
    }
}

int l_ofPattern(lua_State *L)
{
    const char *pattern = lua_tostring(L, 1);

    parsePattern(L, pattern);

    return 0;
}

const struct luaL_Reg libc[] = {
    {"ofPattern", l_ofPattern},

    {NULL, NULL} /* sentinel */
};

int luaopen_liblibc(lua_State *L)
{
    luaL_newlib(L, libc);

    return 1;
}
