

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <time.h>
#include <math.h>
#include <locale.h>
#include <datetimeformatter.h>

#define ERROR_BUFFER_LENGTH (1 << 10) + 1

int l_compile(lua_State *L)
{
    const char *pattern = lua_tostring(L, 1);

    char error[ERROR_BUFFER_LENGTH];

    buffer_t *compiledCode;

    int failed = dtf_compile(pattern, &compiledCode, error);

    if (failed)
    {
        free_buffer(compiledCode);
        luaL_error(L, error);
    }

    lua_createtable(L, compiledCode->length, 0);

    for (int i = 0; i < compiledCode->length; i++)
    {
        lua_pushinteger(L, compiledCode->buffer[i]);
        lua_seti(L, -2, i + 1);
    }

    free_buffer(compiledCode);

    return 1;
}

int l_compile_then_format(lua_State *L)
{
    char output[ERROR_BUFFER_LENGTH];
    buffer_t *pattern;
    int failed;

    const char *str_pattern = lua_tostring(L, 1);
    time_t timer = lua_tointeger(L, 2);
    const char *locale = lua_tostring(L, 3);
    int offset = lua_tointeger(L, 4);
    const char *timezone = lua_tostring(L, 5);
    int local = lua_toboolean(L, 6);

    failed = dtf_compile(str_pattern, &pattern, output);

    if (failed)
    {
        free_buffer(pattern);
        luaL_error(L, output);
    }

    failed = dtf_format(pattern, timer, locale, offset, timezone, local, output);

    free_buffer(pattern);

    if (failed)
        luaL_error(L, output);
    else
        lua_pushstring(L, output);

    return 1;
}

int l_format(lua_State *L)
{
    lua_len(L, 1);
    size_t length = lua_tointeger(L, -1);
    lua_pop(L, 1);

    buffer_t *pattern = new_buffer(length);
    for (int i = 0; i < length; i++)
    {
        lua_geti(L, 1, i + 1);
        add_char(pattern, (char_t)lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    time_t timer = lua_tointeger(L, 2);
    const char *locale = lua_tostring(L, 3);
    int offset = lua_tointeger(L, 4);
    const char *timezone = lua_tostring(L, 5);
    int local = lua_toboolean(L, 6);

    char output[ERROR_BUFFER_LENGTH];
    int failed = dtf_format(pattern, timer, locale, offset, timezone, local, output);

    free_buffer(pattern);

    if (failed)
        luaL_error(L, output);
    else
        lua_pushstring(L, output);

    return 1;
}

int l_mktime(lua_State *L)
{
    int lua_type;
    struct tm info;

    lua_type = lua_getfield(L, 1, "hour");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"hour\" [0,23]");
    info.tm_hour = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "isdst");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"isdst\" {-1, 0, 1}");
    info.tm_isdst = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "mday");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"mday\" [1-31]");
    info.tm_mday = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "min");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"min\" [0-59]");
    info.tm_min = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "mon");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"mon\" [1-12]");
    info.tm_mon = lua_tointeger(L, -1) - 1;
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "sec");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"sec\" [0-60]");
    info.tm_sec = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "wday");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"wday\" [1-7]");
    info.tm_wday = lua_tointeger(L, -1) - 1;
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "yday");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"yday\" [1-366]");
    info.tm_yday = lua_tointeger(L, -1) - 1;
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "year");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"year\" [1900-]");
    info.tm_year = lua_tointeger(L, -1) - 1900;
    lua_pop(L, 1);

    /*
        lua_type = lua_getfield(L, 1, "gmtoff");
        luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"gmtoff\" [0-]");
        info.tm_gmtoff = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_type = lua_getfield(L, 1, "zone");
        luaL_argcheck(L, lua_type == LUA_TSTRING, 1, "Expected an integer for \"zone\" [string]");
        info.tm_zone = lua_tostring(L, -1);
        lua_pop(L, 1);
    */
    time_t timer = mktime(&info);

    lua_pushinteger(L, timer);

    return 1;
}

int l_tm_t(lua_State *L)
{
    time_t timer = lua_tointeger(L, 1);
    bool gm = lua_toboolean(L, 2);

    // const char *locale = lua_tostring(L, 3);

    // assert(setlocale(LC_TIME, locale) != NULL);

    struct tm *info = gm ? gmtime(&timer) : localtime(&timer);

    lua_newtable(L);

    lua_pushinteger(L, info->tm_hour);
    lua_setfield(L, -2, "hour");

    lua_pushinteger(L, info->tm_isdst);
    lua_setfield(L, -2, "isdst");

    lua_pushinteger(L, info->tm_mday);
    lua_setfield(L, -2, "mday");

    lua_pushinteger(L, info->tm_min);
    lua_setfield(L, -2, "min");

    lua_pushinteger(L, info->tm_mon);
    lua_setfield(L, -2, "mon");

    lua_pushinteger(L, info->tm_sec);
    lua_setfield(L, -2, "sec");

    lua_pushinteger(L, info->tm_wday);
    lua_setfield(L, -2, "wday");

    lua_pushinteger(L, info->tm_yday);
    lua_setfield(L, -2, "yday");

    lua_pushinteger(L, info->tm_year);
    lua_setfield(L, -2, "year");

    /*
    lua_pushinteger(L, info->tm_gmtoff);
    lua_setfield(L, -2, "gmtoff");

    lua_pushstring(L, info->tm_zone);
    lua_setfield(L, -2, "zone");
    */

    return 1;
}

int l_time(lua_State *L)
{
    lua_pushinteger(L, time(NULL));
    return 1;
}

const struct luaL_Reg lib[] = {
    {"compile", l_compile},
    {"format", l_format},
    {"compile_then_format", l_compile_then_format},
    {"tm_t_2_timer", l_mktime},
    {"timer_2_tm_t", l_tm_t},
    {"time", l_time},

    {NULL, NULL} /* sentinel */
};

void calendar_fields(lua_State *L)
{
    lua_newtable(L);

    lua_pushinteger(L, ERA);
    lua_setfield(L, -2, "ERA");
    lua_pushinteger(L, YEAR);
    lua_setfield(L, -2, "YEAR");
    lua_pushinteger(L, MONTH);
    lua_setfield(L, -2, "MONTH");
    lua_pushinteger(L, df_DATE);
    lua_setfield(L, -2, "DATE");
    lua_pushinteger(L, HOUR_OF_DAY);
    lua_setfield(L, -2, "HOUR_OF_DAY");
    lua_pushinteger(L, MINUTE);
    lua_setfield(L, -2, "MINUTE");
    lua_pushinteger(L, SECOND);
    lua_setfield(L, -2, "SECOND");
    lua_pushinteger(L, MILLISECOND);
    lua_setfield(L, -2, "MILLISECOND");
    lua_pushinteger(L, DAY_OF_WEEK);
    lua_setfield(L, -2, "DAY_OF_WEEK");
    lua_pushinteger(L, DAY_OF_YEAR);
    lua_setfield(L, -2, "DAY_OF_YEAR");
    lua_pushinteger(L, DAY_OF_WEEK_IN_MONTH);
    lua_setfield(L, -2, "DAY_OF_WEEK_IN_MONTH");
    lua_pushinteger(L, WEEK_OF_YEAR);
    lua_setfield(L, -2, "WEEK_OF_YEAR");
    lua_pushinteger(L, WEEK_OF_MONTH);
    lua_setfield(L, -2, "WEEK_OF_MONTH");
    lua_pushinteger(L, AM_PM);
    lua_setfield(L, -2, "AM_PM");
    lua_pushinteger(L, HOUR);
    lua_setfield(L, -2, "HOUR");
    lua_pushinteger(L, ZONE_OFFSET);
    lua_setfield(L, -2, "ZONE_OFFSET");
    lua_pushinteger(L, WEEK_YEAR);
    lua_setfield(L, -2, "WEEK_YEAR");
    lua_pushinteger(L, ISO_DAY_OF_WEEK);
    lua_setfield(L, -2, "ISO_DAY_OF_WEEK");
    lua_pushinteger(L, DST_OFFSET);
    lua_setfield(L, -2, "DST_OFFSET");

    lua_setfield(L, -2, "calendar_fields");
}

int luaopen_libdatetimeformatterlua(lua_State *L)
{
    luaL_newlib(L, lib);

    return 1;
}
