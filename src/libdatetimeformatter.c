
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

#define STRFTIME_BUFFER_LENGTH 128

#define Long_MAX_VALUE 0x7fffffffffffffffL
#define NANOS_PER_SECOND 1000000000L

#define TAG_QUOTE_ASCII_CHAR 100
#define TAG_QUOTE_CHARS 101

#define PATTERN_ERA 0
#define PATTERN_YEAR 1
#define PATTERN_MONTH 2
#define PATTERN_DAY_OF_MONTH 3
#define PATTERN_HOUR_OF_DAY1 4
#define PATTERN_HOUR_OF_DAY0 5
#define PATTERN_MINUTE 6
#define PATTERN_SECOND 7
#define PATTERN_MILLISECOND 8
#define PATTERN_DAY_OF_WEEK 9
#define PATTERN_DAY_OF_YEAR 10
#define PATTERN_DAY_OF_WEEK_IN_MONTH 11
#define PATTERN_WEEK_OF_YEAR 12
#define PATTERN_WEEK_OF_MONTH 13
#define PATTERN_AM_PM 14
#define PATTERN_HOUR1 15
#define PATTERN_HOUR0 16
#define PATTERN_ZONE_NAME 17
#define PATTERN_ZONE_VALUE 18
#define PATTERN_WEEK_YEAR 19
#define PATTERN_ISO_DAY_OF_WEEK 20
#define PATTERN_ISO_ZONE 21
#define PATTERN_MONTH_STANDALONE 22

#define WEEK_YEAR FIELD_COUNT
#define ISO_DAY_OF_WEEK 1000

static const char *patternChars = "GyMdkHmsSEDFwWahKzZYuXL";
static const char *eras[] = {"BC", "AD"};

typedef struct timespec timespec_t;
typedef struct tm tm_t;

typedef unsigned short char_t;

timespec_t Duration_ofNanos(long nanos)
{
    long secs = nanos / NANOS_PER_SECOND;
    int nos = (int)(nanos % NANOS_PER_SECOND);
    if (nos < 0)
    {
        nos += NANOS_PER_SECOND;
        secs--;
    }

    timespec_t t;
    t.tv_sec = secs;
    t.tv_nsec = nos;
    return t;
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

typedef enum Calendar
{

    // Data flow in Calendar
    // ---------------------

    // The current time is represented in two ways by Calendar: as UTC
    // milliseconds from the epoch (1 January 1970 0:00 UTC), and as local
    // fields such as MONTH, HOUR, AM_PM, etc.  It is possible to compute the
    // millis from the fields, and vice versa.  The data needed to do this
    // conversion is encapsulated by a TimeZone object owned by the Calendar.
    // The data provided by the TimeZone object may also be overridden if the
    // user sets the ZONE_OFFSET and/or DST_OFFSET fields directly. The class
    // keeps track of what information was most recently set by the caller, and
    // uses that to compute any other information as needed.

    // If the user sets the fields using set(), the data flow is as follows.
    // This is implemented by the Calendar subclass's computeTime() method.
    // During this process, certain fields may be ignored.  The disambiguation
    // algorithm for resolving which fields to pay attention to is described
    // in the class documentation.

    //   local fields (YEAR, MONTH, DATE, HOUR, MINUTE, etc.)
    //           |
    //           | Using Calendar-specific algorithm
    //           V
    //   local standard millis
    //           |
    //           | Using TimeZone or user-set ZONE_OFFSET / DST_OFFSET
    //           V
    //   UTC millis (in time data member)

    // If the user sets the UTC millis using setTime() or setTimeInMillis(),
    // the data flow is as follows.  This is implemented by the Calendar
    // subclass's computeFields() method.

    //   UTC millis (in time data member)
    //           |
    //           | Using TimeZone getOffset()
    //           V
    //   local standard millis
    //           |
    //           | Using Calendar-specific algorithm
    //           V
    //   local fields (YEAR, MONTH, DATE, HOUR, MINUTE, etc.)

    // In general, a round trip from fields, through local and UTC millis, and
    // back out to fields is made when necessary.  This is implemented by the
    // complete() method.  Resolving a partial set of fields into a UTC millis
    // value allows all remaining fields to be generated from that value.  If
    // the Calendar is lenient, the fields are also renormalized to standard
    // ranges when they are regenerated.

    /**
     * Field number for {@code get} and {@code set} indicating the
     * era, e.g., AD or BC in the Julian calendar. This is a calendar-specific
     * value; see subclass documentation.
     *
     * @see GregorianCalendar#AD
     * @see GregorianCalendar#BC
     */
    ERA = 0,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * year. This is a calendar-specific value; see subclass documentation.
     */
    YEAR = 1,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * month. This is a calendar-specific value. The first month of
     * the year in the Gregorian and Julian calendars is
     * {@code JANUARY} which is 0; the last depends on the number
     * of months in a year.
     *
     * @see #JANUARY
     * @see #FEBRUARY
     * @see #MARCH
     * @see #APRIL
     * @see #MAY
     * @see #JUNE
     * @see #JULY
     * @see #AUGUST
     * @see #SEPTEMBER
     * @see #OCTOBER
     * @see #NOVEMBER
     * @see #DECEMBER
     * @see #UNDECIMBER
     */
    MONTH = 2,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * week number within the current year.  The first week of the year, as
     * defined by {@code getFirstDayOfWeek()} and
     * {@code getMinimalDaysInFirstWeek()}, has value 1.  Subclasses define
     * the value of {@code WEEK_OF_YEAR} for days before the first week of
     * the year.
     *
     * @see #getFirstDayOfWeek
     * @see #getMinimalDaysInFirstWeek
     */
    WEEK_OF_YEAR = 3,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * week number within the current month.  The first week of the month, as
     * defined by {@code getFirstDayOfWeek()} and
     * {@code getMinimalDaysInFirstWeek()}, has value 1.  Subclasses define
     * the value of {@code WEEK_OF_MONTH} for days before the first week of
     * the month.
     *
     * @see #getFirstDayOfWeek
     * @see #getMinimalDaysInFirstWeek
     */
    WEEK_OF_MONTH = 4,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * day of the month. This is a synonym for {@code DAY_OF_MONTH}.
     * The first day of the month has value 1.
     *
     * @see #DAY_OF_MONTH
     */
    DATE = 5,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * day of the month. This is a synonym for {@code DATE}.
     * The first day of the month has value 1.
     *
     * @see #DATE
     */
    DAY_OF_MONTH = 5,

    /**
     * Field number for {@code get} and {@code set} indicating the day
     * number within the current year.  The first day of the year has value 1.
     */
    DAY_OF_YEAR = 6,

    /**
     * Field number for {@code get} and {@code set} indicating the day
     * of the week.  This field takes values {@code SUNDAY},
     * {@code MONDAY}, {@code TUESDAY}, {@code WEDNESDAY},
     * {@code THURSDAY}, {@code FRIDAY}, and {@code SATURDAY}.
     *
     * @see #SUNDAY
     * @see #MONDAY
     * @see #TUESDAY
     * @see #WEDNESDAY
     * @see #THURSDAY
     * @see #FRIDAY
     * @see #SATURDAY
     */
    DAY_OF_WEEK = 7,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * ordinal number of the day of the week within the current month. Together
     * with the {@code DAY_OF_WEEK} field, this uniquely specifies a day
     * within a month.  Unlike {@code WEEK_OF_MONTH} and
     * {@code WEEK_OF_YEAR}, this field's value does <em>not</em> depend on
     * {@code getFirstDayOfWeek()} or
     * {@code getMinimalDaysInFirstWeek()}.  {@code DAY_OF_MONTH 1}
     * through {@code 7} always correspond to <code>DAY_OF_WEEK_IN_MONTH
     * 1</code>; {@code 8} through {@code 14} correspond to
     * {@code DAY_OF_WEEK_IN_MONTH 2}, and so on.
     * {@code DAY_OF_WEEK_IN_MONTH 0} indicates the week before
     * {@code DAY_OF_WEEK_IN_MONTH 1}.  Negative values count back from the
     * end of the month, so the last Sunday of a month is specified as
     * {@code DAY_OF_WEEK = SUNDAY, DAY_OF_WEEK_IN_MONTH = -1}.  Because
     * negative values count backward they will usually be aligned differently
     * within the month than positive values.  For example, if a month has 31
     * days, {@code DAY_OF_WEEK_IN_MONTH -1} will overlap
     * {@code DAY_OF_WEEK_IN_MONTH 5} and the end of {@code 4}.
     *
     * @see #DAY_OF_WEEK
     * @see #WEEK_OF_MONTH
     */
    DAY_OF_WEEK_IN_MONTH = 8,

    /**
     * Field number for {@code get} and {@code set} indicating
     * whether the {@code HOUR} is before or after noon.
     * E.g., at 10:04:15.250 PM the {@code AM_PM} is {@code PM}.
     *
     * @see #AM
     * @see #PM
     * @see #HOUR
     */
    AM_PM = 9,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * hour of the morning or afternoon. {@code HOUR} is used for the
     * 12-hour clock (0 - 11). Noon and midnight are represented by 0, not by 12.
     * E.g., at 10:04:15.250 PM the {@code HOUR} is 10.
     *
     * @see #AM_PM
     * @see #HOUR_OF_DAY
     */
    HOUR = 10,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * hour of the day. {@code HOUR_OF_DAY} is used for the 24-hour clock.
     * E.g., at 10:04:15.250 PM the {@code HOUR_OF_DAY} is 22.
     *
     * @see #HOUR
     */
    HOUR_OF_DAY = 11,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * minute within the hour.
     * E.g., at 10:04:15.250 PM the {@code MINUTE} is 4.
     */
    MINUTE = 12,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * second within the minute.
     * E.g., at 10:04:15.250 PM the {@code SECOND} is 15.
     */
    SECOND = 13,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * millisecond within the second.
     * E.g., at 10:04:15.250 PM the {@code MILLISECOND} is 250.
     */
    MILLISECOND = 14,

    /**
     * Field number for {@code get} and {@code set}
     * indicating the raw offset from GMT in milliseconds.
     * <p>
     * This field reflects the correct GMT offset value of the time
     * zone of this {@code Calendar} if the
     * {@code TimeZone} implementation subclass supports
     * historical GMT offset changes.
     */
    ZONE_OFFSET = 15,

    /**
     * Field number for {@code get} and {@code set} indicating the
     * daylight saving offset in milliseconds.
     * <p>
     * This field reflects the correct daylight saving offset value of
     * the time zone of this {@code Calendar} if the
     * {@code TimeZone} implementation subclass supports
     * historical Daylight Saving Time schedule changes.
     */
    DST_OFFSET = 16,

    /**
     * The number of distinct fields recognized by {@code get} and {@code set}.
     * Field numbers range from {@code 0..FIELD_COUNT-1}.
     */
    FIELD_COUNT = 17,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Sunday.
     */
    SUNDAY = 1,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Monday.
     */
    MONDAY = 2,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Tuesday.
     */
    TUESDAY = 3,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Wednesday.
     */
    WEDNESDAY = 4,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Thursday.
     */
    THURSDAY = 5,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Friday.
     */
    FRIDAY = 6,

    /**
     * Value of the {@link #DAY_OF_WEEK} field indicating
     * Saturday.
     */
    SATURDAY = 7,

    /**
     * Value of the {@link #MONTH} field indicating the
     * first month of the year in the Gregorian and Julian calendars.
     */
    JANUARY = 0,

    /**
     * Value of the {@link #MONTH} field indicating the
     * second month of the year in the Gregorian and Julian calendars.
     */
    FEBRUARY = 1,

    /**
     * Value of the {@link #MONTH} field indicating the
     * third month of the year in the Gregorian and Julian calendars.
     */
    MARCH = 2,

    /**
     * Value of the {@link #MONTH} field indicating the
     * fourth month of the year in the Gregorian and Julian calendars.
     */
    APRIL = 3,

    /**
     * Value of the {@link #MONTH} field indicating the
     * fifth month of the year in the Gregorian and Julian calendars.
     */
    MAY = 4,

    /**
     * Value of the {@link #MONTH} field indicating the
     * sixth month of the year in the Gregorian and Julian calendars.
     */
    JUNE = 5,

    /**
     * Value of the {@link #MONTH} field indicating the
     * seventh month of the year in the Gregorian and Julian calendars.
     */
    JULY = 6,

    /**
     * Value of the {@link #MONTH} field indicating the
     * eighth month of the year in the Gregorian and Julian calendars.
     */
    AUGUST = 7,

    /**
     * Value of the {@link #MONTH} field indicating the
     * ninth month of the year in the Gregorian and Julian calendars.
     */
    SEPTEMBER = 8,

    /**
     * Value of the {@link #MONTH} field indicating the
     * tenth month of the year in the Gregorian and Julian calendars.
     */
    OCTOBER = 9,

    /**
     * Value of the {@link #MONTH} field indicating the
     * eleventh month of the year in the Gregorian and Julian calendars.
     */
    NOVEMBER = 10,

    /**
     * Value of the {@link #MONTH} field indicating the
     * twelfth month of the year in the Gregorian and Julian calendars.
     */
    DECEMBER = 11,

    /**
     * Value of the {@link #MONTH} field indicating the
     * thirteenth month of the year. Although {@code GregorianCalendar}
     * does not use this value, lunar calendars do.
     */
    UNDECIMBER = 12,

    /**
     * Value of the {@link #AM_PM} field indicating the
     * period of the day from midnight to just before noon.
     */
    AM = 0,

    /**
     * Value of the {@link #AM_PM} field indicating the
     * period of the day from noon to just before midnight.
     */
    PM = 1,

    /**
     * A style specifier for {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating names in all styles, such as
     * "January" and "Jan".
     *
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @see #SHORT_STANDALONE
     * @see #LONG_STANDALONE
     * @see #SHORT
     * @see #LONG
     * @since 1.6
     */
    ALL_STYLES = 0,

    STANDALONE_MASK = 0x8000,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} equivalent to {@link #SHORT_FORMAT}.
     *
     * @see #SHORT_STANDALONE
     * @see #LONG
     * @since 1.6
     */
    SHORT = 1,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} equivalent to {@link #LONG_FORMAT}.
     *
     * @see #LONG_STANDALONE
     * @see #SHORT
     * @since 1.6
     */
    LONG = 2,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a narrow name used for format. Narrow names
     * are typically single character strings, such as "M" for Monday.
     *
     * @see #NARROW_STANDALONE
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @since 1.8
     */
    NARROW_FORMAT = 4,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a narrow name independently. Narrow names
     * are typically single character strings, such as "M" for Monday.
     *
     * @see #NARROW_FORMAT
     * @see #SHORT_STANDALONE
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    // NARROW_STANDALONE = NARROW_FORMAT | STANDALONE_MASK,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a short name used for format.
     *
     * @see #SHORT_STANDALONE
     * @see #LONG_FORMAT
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    SHORT_FORMAT = 1,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a long name used for format.
     *
     * @see #LONG_STANDALONE
     * @see #SHORT_FORMAT
     * @see #SHORT_STANDALONE
     * @since 1.8
     */
    LONG_FORMAT = 2,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a short name used independently,
     * such as a month abbreviation as calendar headers.
     *
     * @see #SHORT_FORMAT
     * @see #LONG_FORMAT
     * @see #LONG_STANDALONE
     * @since 1.8
     */
    // SHORT_STANDALONE = SHORT | STANDALONE_MASK,

    /**
     * A style specifier for {@link #getDisplayName(int, int, Locale)
     * getDisplayName} and {@link #getDisplayNames(int, int, Locale)
     * getDisplayNames} indicating a long name used independently,
     * such as a month name as calendar headers.
     *
     * @see #LONG_FORMAT
     * @see #SHORT_FORMAT
     * @see #SHORT_STANDALONE
     * @since 1.8
     */
    // LONG_STANDALONE = LONG | STANDALONE_MASK,
} calendar_t;

static calendar_t PATTERN_INDEX_TO_CALENDAR_FIELD[] = {
    ERA,
    YEAR,
    MONTH,
    DATE,
    HOUR_OF_DAY,
    HOUR_OF_DAY,
    MINUTE,
    SECOND,
    MILLISECOND,
    DAY_OF_WEEK,
    DAY_OF_YEAR,
    DAY_OF_WEEK_IN_MONTH,
    WEEK_OF_YEAR,
    WEEK_OF_MONTH,
    AM_PM,
    HOUR,
    HOUR,
    ZONE_OFFSET,
    ZONE_OFFSET,
    WEEK_YEAR,       // Pseudo Calendar field
    ISO_DAY_OF_WEEK, // Pseudo Calendar field
    ZONE_OFFSET,
    MONTH};

typedef enum SignStyle
{

    /**
     * Style to output the sign only if the value is negative.
     * <p>
     * In strict parsing, the negative sign will be accepted and the positive sign rejected.
     * In lenient parsing, any sign will be accepted.
     */
    NORMAL,
    /**
     * Style to always output the sign, where zero will output '+'.
     * <p>
     * In strict parsing, the absence of a sign will be rejected.
     * In lenient parsing, any sign will be accepted, with the absence
     * of a sign treated as a positive number.
     */
    ALWAYS,
    /**
     * Style to never output sign, only outputting the absolute value.
     * <p>
     * In strict parsing, any sign will be rejected.
     * In lenient parsing, any sign will be accepted unless the width is fixed.
     */
    NEVER,
    /**
     * Style to block negative values, throwing an exception on printing.
     * <p>
     * In strict parsing, any sign will be rejected.
     * In lenient parsing, any sign will be accepted unless the width is fixed.
     */
    NOT_NEGATIVE,
    /**
     * Style to always output the sign if the value exceeds the pad width.
     * A negative value will always output the '-' sign.
     * <p>
     * In strict parsing, the sign will be rejected unless the pad width is exceeded.
     * In lenient parsing, any sign will be accepted, with the absence
     * of a sign treated as a positive number.
     */
    EXCEEDS_PAD,
} signstyle_t;

typedef struct buffer_s
{
    size_t size;
    size_t length;
    char_t *buffer;
} buffer_t;

buffer_t *new_buffer(size_t size)
{
    buffer_t *b = (buffer_t *)malloc(sizeof(buffer_t));

    b->size = size;
    b->length = 0;
    b->buffer = (char_t *)malloc(sizeof(char_t) * size);

    return b;
}

void free_buffer(buffer_t *B)
{
    if (B != NULL)
    {
        free(B->buffer);
        free(B);
    }
}

void add_char(buffer_t *B, char_t c)
{
    assert(B->length < B->size);

    B->buffer[B->length++] = c;
}

void add_buffer(buffer_t *B, buffer_t *C)
{
    int l = C->length;
    char_t *another = C->buffer;

    for (int i = 0; i < l; i++)
        add_char(B, another[i]);
}

int triple_shift(int n, int s)
{
    return n >= 0 ? n >> s : (n >> s) + (2 << ~s);
}

void encode(lua_State *L, int tag, int length, buffer_t *buffer)
{
    if (tag == PATTERN_ISO_ZONE && length >= 4)
    {
        luaL_error(L, "invalid ISO 8601 format: length=%d", length);
    }
    if (length < 255)
    {
        add_char(buffer, (char_t)(tag << 8 | length));
    }
    else
    {
        add_char(buffer, (char_t)((tag << 8) | 0xff));
        add_char(buffer, (char_t)triple_shift(length, 16));
        add_char(buffer, (char_t)(length & 0xffff));
    }
}

int l_compile(lua_State *L)
{
    const char *pattern = lua_tostring(L, 1);

    int length = strlen(pattern);

    bool inQuote = false;

    buffer_t *compiledCode = new_buffer(length * 2); // new StringBuilder(length * 2);
    buffer_t *tmpBuffer = NULL;

    int count = 0, tagcount = 0;
    int lastTag = -1; //, prevTag = -1;

    for (int i = 0; i < length; i++)
    {
        char c = pattern[i];

        if (c == '\'')
        {
            // '' is treated as a single quote regardless of being in a quoted section.
            // if ((i + 1) < length)
            if (i < length - 1)
            {
                c = pattern[i + 1];
                if (c == '\'')
                {
                    i++;
                    if (count != 0)
                    {
                        encode(L, lastTag, count, compiledCode);
                        tagcount++;
                        // prevTag = lastTag;
                        lastTag = -1;
                        count = 0;
                    }
                    if (inQuote)
                    {
                        add_char(tmpBuffer, (char_t)c);
                    }
                    else
                    {
                        add_char(compiledCode, (char_t)(TAG_QUOTE_ASCII_CHAR << 8 | c));
                    }
                    continue;
                }
            }
            if (!inQuote)
            {
                if (count != 0)
                {
                    encode(L, lastTag, count, compiledCode);
                    tagcount++;
                    // prevTag = lastTag;
                    lastTag = -1;
                    count = 0;
                }
                if (tmpBuffer == NULL)
                {
                    tmpBuffer = new_buffer(length); // new StringBuilder(length);
                }
                else
                {
                    tmpBuffer->length = 0; // tmpBuffer.setLength(0);
                }
                inQuote = true;
            }
            else
            {
                int len = tmpBuffer->length;
                if (len == 1)
                {
                    char_t ch = tmpBuffer->buffer[0];
                    if (ch < 128)
                    {
                        add_char(compiledCode, (char_t)(TAG_QUOTE_ASCII_CHAR << 8 | ch));
                    }
                    else
                    {
                        add_char(compiledCode, (char_t)(TAG_QUOTE_CHARS << 8 | 1));
                        add_char(compiledCode, ch);
                    }
                }
                else
                {
                    encode(L, TAG_QUOTE_CHARS, len, compiledCode);
                    add_buffer(compiledCode, tmpBuffer);
                }
                inQuote = false;
            }
            continue;
        }
        if (inQuote)
        {
            add_char(tmpBuffer, (char_t)c);
            continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        {
            if (count != 0)
            {
                encode(L, lastTag, count, compiledCode);
                tagcount++;
                // prevTag = lastTag;
                lastTag = -1;
                count = 0;
            }
            if (c < 128)
            {
                // In most cases, c would be a delimiter, such as ':'.
                add_char(compiledCode, (char_t)(TAG_QUOTE_ASCII_CHAR << 8 | c));
            }
            else
            {
                // Take any contiguous non-ASCII alphabet characters and
                // put them in a single TAG_QUOTE_CHARS.
                int j;
                for (j = i + 1; j < length; j++)
                {
                    char d = pattern[j];
                    if (d == '\'' || ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z')))
                    {
                        break;
                    }
                }
                encode(L, TAG_QUOTE_CHARS, j - i, compiledCode);
                for (; i < j; i++)
                {
                    add_char(compiledCode, (char_t)pattern[i]);
                }
                i--;
            }
            continue;
        }

        int tag = strchr(patternChars, c) - patternChars;
        if (tag < 0)
        {
            luaL_error(L, "Illegal pattern character '%c'", c);
        }
        if (lastTag == -1 || lastTag == tag)
        {
            lastTag = tag;
            count++;
            continue;
        }
        encode(L, lastTag, count, compiledCode);
        tagcount++;
        // prevTag = lastTag;
        lastTag = tag;
        count = 1;
    }

    if (inQuote)
    {
        luaL_error(L, "Unterminated quote");
    }

    if (count != 0)
    {
        encode(L, lastTag, count, compiledCode);
        tagcount++;
        // prevTag = lastTag;
    }

    // bool forceStandaloneForm = (tagcount == 1 && prevTag == PATTERN_MONTH);

    free_buffer(tmpBuffer);

    lua_pushlightuserdata(L, compiledCode);

    lua_createtable(L, compiledCode->length, 0);

    for (int i = 0; i < compiledCode->length; i++)
    {
        // printf("cp at %d is %hu\n", i, compiledCode->buffer[i]);

        lua_pushinteger(L, compiledCode->buffer[i]);
        lua_seti(L, -2, i + 1);
    }

    // lua_pushboolean(L, forceStandaloneForm);

    return 2;
}

typedef enum DateFormat
{
    /**
     * Useful constant for ERA field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    ERA_FIELD = 0,
    /**
     * Useful constant for YEAR field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    YEAR_FIELD = 1,
    /**
     * Useful constant for MONTH field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    MONTH_FIELD = 2,
    /**
     * Useful constant for DATE field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    DATE_FIELD = 3,
    /**
     * Useful constant for one-based HOUR_OF_DAY field alignment.
     * Used in FieldPosition of date/time formatting.
     * HOUR_OF_DAY1_FIELD is used for the one-based 24-hour clock.
     * For example, 23:59 + 01:00 results in 24:59.
     */
    HOUR_OF_DAY1_FIELD = 4,
    /**
     * Useful constant for zero-based HOUR_OF_DAY field alignment.
     * Used in FieldPosition of date/time formatting.
     * HOUR_OF_DAY0_FIELD is used for the zero-based 24-hour clock.
     * For example, 23:59 + 01:00 results in 00:59.
     */
    HOUR_OF_DAY0_FIELD = 5,
    /**
     * Useful constant for MINUTE field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    MINUTE_FIELD = 6,
    /**
     * Useful constant for SECOND field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    SECOND_FIELD = 7,
    /**
     * Useful constant for MILLISECOND field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    MILLISECOND_FIELD = 8,
    /**
     * Useful constant for DAY_OF_WEEK field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    DAY_OF_WEEK_FIELD = 9,
    /**
     * Useful constant for DAY_OF_YEAR field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    DAY_OF_YEAR_FIELD = 10,
    /**
     * Useful constant for DAY_OF_WEEK_IN_MONTH field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    DAY_OF_WEEK_IN_MONTH_FIELD = 11,
    /**
     * Useful constant for WEEK_OF_YEAR field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    WEEK_OF_YEAR_FIELD = 12,
    /**
     * Useful constant for WEEK_OF_MONTH field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    WEEK_OF_MONTH_FIELD = 13,
    /**
     * Useful constant for AM_PM field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    AM_PM_FIELD = 14,
    /**
     * Useful constant for one-based HOUR field alignment.
     * Used in FieldPosition of date/time formatting.
     * HOUR1_FIELD is used for the one-based 12-hour clock.
     * For example, 11:30 PM + 1 hour results in 12:30 AM.
     */
    HOUR1_FIELD = 15,
    /**
     * Useful constant for zero-based HOUR field alignment.
     * Used in FieldPosition of date/time formatting.
     * HOUR0_FIELD is used for the zero-based 12-hour clock.
     * For example, 11:30 PM + 1 hour results in 00:30 AM.
     */
    HOUR0_FIELD = 16,
    /**
     * Useful constant for TIMEZONE field alignment.
     * Used in FieldPosition of date/time formatting.
     */
    TIMEZONE_FIELD = 17,
} dateformat_t;

// static const dateformat_t *PATTERN_INDEX_TO_DATE_FORMAT_FIELD = {
//     ERA_FIELD,
//     YEAR_FIELD,
//     MONTH_FIELD,
//     DATE_FIELD,
//     HOUR_OF_DAY1_FIELD,
//     HOUR_OF_DAY0_FIELD,
//     MINUTE_FIELD,
//     SECOND_FIELD,
//     MILLISECOND_FIELD,
//     DAY_OF_WEEK_FIELD,
//     DAY_OF_YEAR_FIELD,
//     DAY_OF_WEEK_IN_MONTH_FIELD,
//     WEEK_OF_YEAR_FIELD,
//     WEEK_OF_MONTH_FIELD,
//     AM_PM_FIELD,
//     HOUR1_FIELD,
//     HOUR0_FIELD,
//     TIMEZONE_FIELD,
//     TIMEZONE_FIELD,
//     YEAR_FIELD,
//     DAY_OF_WEEK_FIELD,
//     TIMEZONE_FIELD,
//     MONTH_FIELD};

// typedef struct Field
// {
//     const char *name;
//     calendar_t calendarField;
// } field_t;

// static field_t PATTERN_INDEX_TO_DATE_FORMAT_FIELD_ID[] = {
//     {"era", ERA},
//     {"year", YEAR},
//     {"month", MONTH},
//     {"day of month", DAY_OF_MONTH},
//     {"hour of day 1", -1},
//     {"hour of day", HOUR_OF_DAY},
//     {"minute", MINUTE},
//     {"second", SECOND},
//     {"millisecond", MILLISECOND},
//     {"day of week", DAY_OF_WEEK},
//     {"day of year", DAY_OF_YEAR},
//     {"day of week in month", DAY_OF_WEEK_IN_MONTH},
//     {"week of year", WEEK_OF_YEAR},
//     {"week of month", WEEK_OF_MONTH},
//     {"am pm", AM_PM},
//     {"hour 1", -1},
//     {"hour", HOUR},
//     {"time zone", -1},
//     {"time zone", -1},
//     {"year", YEAR},
//     {"day of week", DAY_OF_WEEK},
//     {"time zone", -1},
//     {"month", MONTH}};

// void formatted(int fieldID, field_t attr, int value, int start, int end, luaL_Buffer *buffer)
// {
// }

// int calendar_get(lua_State *L, int date_table_index, int field)
// {
//     int lua_type = lua_geti(L, date_table_index, field);
//     assert(lua_type == LUA_TNUMBER);
//     int value = lua_tointeger(L, -1);
//     lua_pop(L, 1);
//     return value;
// }

int calendar_get(lua_State *L, tm_t *info, int field)
{
    int v = -1;

    switch (field)
    {
    case ERA:
        v = info->tm_year + 1900 >= 0 ? 1 : 0;
        break;
    case YEAR:
        v = info->tm_year + 1900;
        break;
    case MONTH:
        v = info->tm_mon;
        break;
    case DATE:
        v = info->tm_mday;
        break;
    case HOUR_OF_DAY:
        v = info->tm_hour;
        break;
    case MINUTE:
        v = info->tm_min;
        break;
    case SECOND:
        v = info->tm_sec;
        break;
    case MILLISECOND:
        luaL_error(L, "MILLISECOND calendar field isn't supported.");
    case DAY_OF_WEEK:
        v = info->tm_wday;
        break;
    case DAY_OF_YEAR:
        v = info->tm_yday + 1;
        break;
    case DAY_OF_WEEK_IN_MONTH:
        luaL_error(L, "DAY_OF_WEEK_IN_MONTH calendar field isn't supported.");
    case WEEK_OF_YEAR:
        luaL_error(L, "WEEK_OF_YEAR calendar field isn't supported.");
    case WEEK_OF_MONTH:
        luaL_error(L, "WEEK_OF_MONTH calendar field isn't supported.");
    case AM_PM:
        v = info->tm_hour < 12 ? 0 : 1;
        break;
    case HOUR:
        v = info->tm_hour;
        break;
    case ZONE_OFFSET:
        v = info->tm_gmtoff;
        break;
    case WEEK_YEAR:
        luaL_error(L, "WEEK_YEAR calendar field isn't supported.");
    case ISO_DAY_OF_WEEK:
        v = info->tm_wday + 1;
        break;
    case DST_OFFSET:
        v = info->tm_isdst;
        break;
    default:
        luaL_error(L, "Generic calendar field %d isn't supported.", field);
    }
    return v;
}

// void calendar_getfield_at(lua_State *L, int date_table_index, const char *field, int value, char **current)
// {
//     lua_getfield(L, date_table_index, field);
//     lua_len(L, -1);
//     int length = lua_tointeger(L, -1);
//     int n = 2; // 2 values to pop from the stack.

//     if (value < length)
//     {
//         // current = eras[value];
//         int lua_type = lua_geti(L, -2, value + 1);
//         assert(lua_type == LUA_TSTRING);
//         *current = (char *)lua_tostring(L, -1);
//         n++; // one more string to be popped out.
//     }

//     lua_pop(L, n);
// }

void sprintf0d(luaL_Buffer *sb, int value, int width)
{
    long d = value;
    if (d < 0)
    {
        luaL_addchar(sb, '-');
        d = -d;
        --width;
    }
    int n = 10;
    for (int i = 2; i < width; i++)
    {
        n *= 10;
    }
    for (int i = 1; i < width && d < n; i++)
    {
        luaL_addchar(sb, '0');
        n /= 10;
    }
    luaL_addchar(sb, (char)d);
}

int toISODayOfWeek(int calendarDayOfWeek)
{
    return calendarDayOfWeek == SUNDAY ? 7 : calendarDayOfWeek - 1;
}

// #define ONE_SECOND 1000;
// #define ONE_MINUTE 60 * ONE_SECOND
// #define ONE_HOUR 60 * ONE_MINUTE
// #define ONE_DAY 24 * ONE_HOUR
// #define ONE_WEEK 7 * ONE_DAY
// #define ZONE_OFFSET 14 * ONE_HOUR
// #define DST_OFFSET 20 * ONE_MINUTE

static const int LEAST_MAX_VALUES[] = {
    1,         // ERA
    292269054, // YEAR
    DECEMBER,  // MONTH
    52,        // WEEK_OF_YEAR
    4,         // WEEK_OF_MONTH
    28,        // DAY_OF_MONTH
    365,       // DAY_OF_YEAR
    SATURDAY,  // DAY_OF_WEEK
    4,         // DAY_OF_WEEK_IN
    PM,        // AM_PM
    11,        // HOUR
    23,        // HOUR_OF_DAY
    59,        // MINUTE
    59,        // SECOND
    999,       // MILLISECOND
    // ZONE_OFFSET, // ZONE_OFFSET
    // DST_OFFSET,   // DST_OFFSET (historical least maximum)
};

static const int MAX_VALUES[] = {
    1,         // ERA
    292278994, // YEAR
    DECEMBER,  // MONTH
    53,        // WEEK_OF_YEAR
    6,         // WEEK_OF_MONTH
    31,        // DAY_OF_MONTH
    366,       // DAY_OF_YEAR
    SATURDAY,  // DAY_OF_WEEK
    6,         // DAY_OF_WEEK_IN
    PM,        // AM_PM
    11,        // HOUR
    23,        // HOUR_OF_DAY
    59,        // MINUTE
    59,        // SECOND
    999,       // MILLISECOND
    // 14 * ONE_HOUR, // ZONE_OFFSET
    // 2 * ONE_HOUR   // DST_OFFSET (double summer time)
};

int calendar_getLeastMaximum(int i) { return LEAST_MAX_VALUES[i]; }

int calendar_getMaximum(int i) { return MAX_VALUES[i]; }

void zeroPaddingNumber(lua_State *L, int value, int minDigits, int maxDigits, luaL_Buffer *buffer)
{
    // Optimization for 1, 2 and 4 digit numbers. This should
    // cover most cases of formatting date/time related items.
    // Note: This optimization code assumes that maxDigits is
    // either 2 or Integer.MAX_VALUE (maxIntCount in format()).

    // try
    {
        char zeroDigit = 0;
        if (zeroDigit == 0)
        {
            // zeroDigit = ((DecimalFormat)numberFormat).getDecimalFormatSymbols().getZeroDigit();
            zeroDigit = '0';
        }
        if (value >= 0)
        {
            if (value < 100 && minDigits >= 1 && minDigits <= 2)
            {
                if (value < 10)
                {
                    if (minDigits == 2)
                    {
                        luaL_addchar(buffer, zeroDigit);
                    }
                    luaL_addchar(buffer, (char)(zeroDigit + value));
                }
                else
                {
                    luaL_addchar(buffer, (char)(zeroDigit + value / 10));
                    luaL_addchar(buffer, (char)(zeroDigit + value % 10));
                }
                return;
            }
            else if (value >= 1000 && value < 10000)
            {
                if (minDigits == 4)
                {
                    luaL_addchar(buffer, (char)(zeroDigit + value / 1000));
                    value %= 1000;
                    luaL_addchar(buffer, (char)(zeroDigit + value / 100));
                    value %= 100;
                    luaL_addchar(buffer, (char)(zeroDigit + value / 10));
                    luaL_addchar(buffer, (char)(zeroDigit + value % 10));
                    return;
                }
                if (minDigits == 2 && maxDigits == 2)
                {
                    zeroPaddingNumber(L, value % 100, 2, 2, buffer);
                    return;
                }
            }
        }
    }
    // catch (Exception e)
    // {
    // }

    // numberFormat.setMinimumIntegerDigits(minDigits);
    // numberFormat.setMaximumIntegerDigits(maxDigits);
    // numberFormat.format((long)value, buffer, DontCareFieldPosition.INSTANCE);
    lua_pushinteger(L, value);
    luaL_addvalue(buffer);
}

void subFormat(lua_State *L, int date_table_index, tm_t *info, int patternCharIndex, int count, luaL_Buffer *buffer)
{
    // int lua_type;
    char strftime_buffer[STRFTIME_BUFFER_LENGTH];

    int maxIntCount = INT_MAX;
    char *current = NULL;
    // int beginOffset = luaL_bufflen(buffer);

    int field = PATTERN_INDEX_TO_CALENDAR_FIELD[patternCharIndex];
    int value;
    if (field == WEEK_YEAR)
    {
        // if (calendar.isWeekDateSupported())
        // lua_type = lua_getfield(L, date_table_index, "isWeekDateSupported");
        // if (lua_type == LUA_TBOOLEAN && lua_toboolean(L, -1))
        // {
        //     // value = calendar.getWeekYear();
        //     lua_type = lua_getfield(L, date_table_index, "getWeekYear");
        //     assert(lua_type == LUA_TNUMBER);
        //     value = lua_tointeger(L, -1);
        //     lua_pop(L, 1);
        // }
        // else
        {
            // use calendar year 'y' instead
            patternCharIndex = PATTERN_YEAR;
            field = PATTERN_INDEX_TO_CALENDAR_FIELD[patternCharIndex];
            value = calendar_get(L, info, field);
        }
        // lua_pop(L, 1);
    }
    else if (field == ISO_DAY_OF_WEEK)
    {
        value = toISODayOfWeek(calendar_get(L, info, DAY_OF_WEEK));
    }
    else
    {
        value = calendar_get(L, info, field);
    }

    // int style = (count >= 4) ? LONG : SHORT;
    // if (!useDateFormatSymbols && field < ZONE_OFFSET && patternCharIndex != PATTERN_MONTH_STANDALONE)
    // {
    //     current = calendar.getDisplayName(field, style, locale);
    // }

    // Note: zeroPaddingNumber(L, ) assumes that maxDigits is either
    // 2 or maxIntCount. If we make any changes to this,
    // zeroPaddingNumber(L, ) must be fixed.

    switch (patternCharIndex)
    {
    case PATTERN_ERA: // 'G'
        // if (useDateFormatSymbols)
        {
            // const char **eras = formatData.getEras();
            // calendar_getfield_at(L, date_table_index, "getEras", value, &current);
            current = eras[value];
        }
        if (current == NULL)
        {
            current = "";
        }
        break;

    case PATTERN_WEEK_YEAR: // 'Y'
    case PATTERN_YEAR:      // 'y'
        // lua_type = lua_getfield(L, date_table_index, "isGregorianCalendar");
        // if (lua_type == LUA_TBOOLEAN && lua_toboolean(L, -1))
        {
            if (count != 2)
            {
                zeroPaddingNumber(L, value, count, maxIntCount, buffer);
            }
            else
            {
                zeroPaddingNumber(L, value, 2, 2, buffer);
            } // clip 1996 to 96
        }
        // else
        // {
        //     if (current == NULL)
        //     {
        //         zeroPaddingNumber(L, value, style == LONG ? 1 : count, maxIntCount, buffer);
        //     }
        // }
        // lua_pop(L, 1);
        break;

    case PATTERN_MONTH_STANDALONE: // 'L'
    case PATTERN_MONTH:            // 'M' (context sensitive)
        // if (useDateFormatSymbols)
        {
            if (count >= 4)
            {
                // months = formatData.getMonths();
                // current = months[value];
                // calendar_getfield_at(L, date_table_index, "getMonths", value, &current);

                strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%B", info);
                current = strftime_buffer;
            }
            else if (count == 3)
            {
                // months = formatData.getShortMonths();
                // current = months[value];
                // calendar_getfield_at(L, date_table_index, "getShortMonths", value, &current);
                strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%b", info);
                current = strftime_buffer;
            }
        }
        // else
        // {
        //     if (count < 3)
        //     {
        //         current = NULL;
        //     }
        //     else if (forceStandaloneForm)
        //     {
        //         current = calendar.getDisplayName(field, style | 0x8000, locale);
        //         if (current == NULL)
        //         {
        //             current = calendar.getDisplayName(field, style, locale);
        //         }
        //     }
        // }
        if (current == NULL)
        {
            zeroPaddingNumber(L, value + 1, count, maxIntCount, buffer);
        }
        break;

        // case PATTERN_MONTH_STANDALONE: // 'L'
        //     assert(current == NULL);
        //     // if (locale == NULL)
        //     {
        //         const char **months;
        //         if (count >= 4)
        //         {
        //             months = formatData.getMonths();
        //             current = months[value];
        //         }
        //         else if (count == 3)
        //         {
        //             months = formatData.getShortMonths();
        //             current = months[value];
        //         }
        //     }
        //     // else
        //     // {
        //     //     if (count >= 3)
        //     //     {
        //     //         current = calendar.getDisplayName(field, style | 0x8000, locale);
        //     //     }
        //     // }
        //     if (current == NULL)
        //     {
        //         zeroPaddingNumber(L, value + 1, count, maxIntCount, buffer);
        //     }
        //     break;

    case PATTERN_HOUR_OF_DAY1: // 'k' 1-based.  eg, 23:59 + 1 hour =>> 24:59
        if (current == NULL)
        {
            if (value == 0)
            {
                zeroPaddingNumber(L, calendar_getMaximum(HOUR_OF_DAY) + 1, count, maxIntCount, buffer);
            }
            else
            {
                zeroPaddingNumber(L, value, count, maxIntCount, buffer);
            }
        }
        break;

    case PATTERN_DAY_OF_WEEK: // 'E'
        // if (useDateFormatSymbols)
        {
            if (count >= 4)
            {
                // weekdays = formatData.getWeekdays();
                // current = weekdays[value];
                // calendar_getfield_at(L, date_table_index, "getWeekdays", value, &current);
                strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%A", info);
                current = strftime_buffer;
            }
            else
            { // count < 4, use abbreviated form if exists
                // weekdays = formatData.getShortWeekdays();
                // current = weekdays[value];
                // calendar_getfield_at(L, date_table_index, "getShortWeekdays", value, &current);
                strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%a", info);
                current = strftime_buffer;
            }
        }
        break;

    case PATTERN_AM_PM: // 'a'
        // if (useDateFormatSymbols)
        {
            // const char **ampm = formatData.getAmPmStrings();
            // current = ampm[value];
            // calendar_getfield_at(L, date_table_index, "getAmPmStrings", value, &current);
            strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%p", info);
            current = strftime_buffer;
        }
        break;

    case PATTERN_HOUR1: // 'h' 1-based.  eg, 11PM + 1 hour =>> 12 AM
        if (current == NULL)
        {
            if (value == 0)
            {
                zeroPaddingNumber(L, calendar_getLeastMaximum(HOUR) + 1, count, maxIntCount, buffer);
            }
            else
            {
                zeroPaddingNumber(L, value, count, maxIntCount, buffer);
            }
        }
        break;

    case PATTERN_ZONE_NAME: // 'z'
        if (current == NULL)
        {
            // if (formatData.locale == NULL || formatData.isZoneStringsSet)
            // {
            //     int zoneIndex =
            //         formatData.getZoneIndex(calendar.getTimeZone().getID());
            //     if (zoneIndex == -1)
            //     {
            //         value = calendar_get(L, date_table_index, ZONE_OFFSET) +
            //                 calendar_get(L, date_table_index, DST_OFFSET);
            //         buffer.append(ZoneInfoFile.toCustomID(value));
            //     }
            //     else
            //     {
            //         int index = (calendar_get(L, date_table_index, DST_OFFSET) == 0) ? 1 : 3;
            //         if (count < 4)
            //         {
            //             // Use the short name
            //             index++;
            //         }
            //         String[][] zoneStrings = formatData.getZoneStringsWrapper();
            //         buffer.append(zoneStrings[zoneIndex][index]);
            //     }
            // }
            // else
            {
                // TimeZone tz = calendar.getTimeZone();
                // int tzstyle = (count < 4 ? TimeZone.SHORT : TimeZone.LONG);
                // buffer.append(tz.getDisplayName(daylight, tzstyle, formatData.locale));
                // bool daylight = (calendar_get(L, date_table_index, DST_OFFSET) != 0);

                // char *s;
                // if (count >= 4)
                // {
                //     calendar_getfield_at(L, date_table_index, "getTimeZone", 1, &s);
                // }
                // else
                // {
                //     calendar_getfield_at(L, date_table_index, "getShortTimeZone", 1, &s);
                // }
                strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%Z", info);
                luaL_addstring(buffer, strftime_buffer);
            }
        }
        break;

    case PATTERN_ZONE_VALUE: // 'Z' ("-/+hhmm" form)
        // value = (calendar_get(L, info, ZONE_OFFSET) + calendar_get(L, info, DST_OFFSET)) / 60000;

        // int width = 4;
        // if (value >= 0)
        // {
        //     add_char(buffer, '+');
        // }
        // else
        // {
        //     width++;
        // }

        // int num = (value / 60) * 100 + (value % 60);
        // sprintf0d(buffer, num, width);

        strftime(strftime_buffer, STRFTIME_BUFFER_LENGTH, "%z", info);
        luaL_addstring(buffer, strftime_buffer);

        break;

    case PATTERN_ISO_ZONE: // 'X'
        value = calendar_get(L, info, ZONE_OFFSET) + calendar_get(L, info, DST_OFFSET);

        if (value == 0)
        {
            luaL_addchar(buffer, 'Z');
            break;
        }

        value /= 60000;
        if (value >= 0)
        {
            luaL_addchar(buffer, '+');
        }
        else
        {
            luaL_addchar(buffer, '-');
            value = -value;
        }

        sprintf0d(buffer, value / 60, 2);
        if (count == 1)
        {
            break;
        }

        if (count == 3)
        {
            luaL_addchar(buffer, ':');
        }
        sprintf0d(buffer, value % 60, 2);
        break;

    default:
        // case PATTERN_DAY_OF_MONTH:         // 'd'
        // case PATTERN_HOUR_OF_DAY0:         // 'H' 0-based.  eg, 23:59 + 1 hour =>> 00:59
        // case PATTERN_MINUTE:               // 'm'
        // case PATTERN_SECOND:               // 's'
        // case PATTERN_MILLISECOND:          // 'S'
        // case PATTERN_DAY_OF_YEAR:          // 'D'
        // case PATTERN_DAY_OF_WEEK_IN_MONTH: // 'F'
        // case PATTERN_WEEK_OF_YEAR:         // 'w'
        // case PATTERN_WEEK_OF_MONTH:        // 'W'
        // case PATTERN_HOUR0:                // 'K' eg, 11PM + 1 hour =>> 0 AM
        // case PATTERN_ISO_DAY_OF_WEEK:      // 'u' pseudo field, Monday = 1, ..., Sunday = 7
        if (current == NULL)
        {
            zeroPaddingNumber(L, value, count, maxIntCount, buffer);
        }
        break;
    } // switch (patternCharIndex)

    if (current != NULL)
    {
        luaL_addstring(buffer, current);
    }

    // int fieldID = PATTERN_INDEX_TO_DATE_FORMAT_FIELD[patternCharIndex];
    // field_t f = PATTERN_INDEX_TO_DATE_FORMAT_FIELD_ID[patternCharIndex];

    // formatted(fieldID, f, f, beginOffset, luaL_bufflen(buffer), buffer);
}

int format(lua_State *L, buffer_t *compiledPattern, tm_t *info, int date_table_index)
{
    luaL_Buffer toAppendTo;
    luaL_buffinit(L, &toAppendTo);

    for (int i = 0; i < compiledPattern->length;)
    {
        int tag = triple_shift(compiledPattern->buffer[i], 8);
        int count = compiledPattern->buffer[i++] & 0xff;
        if (count == 255)
        {
            count = compiledPattern->buffer[i++] << 16;
            count |= compiledPattern->buffer[i++];
        }

        switch (tag)
        {
        case TAG_QUOTE_ASCII_CHAR:
            luaL_addchar(&toAppendTo, (char)count);
            break;

        case TAG_QUOTE_CHARS:
            // luaL_addlstring(&toAppendTo, (char *)(compiledPattern->buffer + i), count);
            char_t *shifted = compiledPattern->buffer + i;
            for (int j = 0; j < count; j++)
            {
                luaL_addchar(&toAppendTo, (char)shifted[j]);
            }
            i += count;
            break;

        default:
            subFormat(L, date_table_index, info, tag, count, &toAppendTo);
            break;
        }
    }

    luaL_pushresult(&toAppendTo);
    return 1;
}

int l_format(lua_State *L)
{
    buffer_t *pattern = (buffer_t *)lua_touserdata(L, 1);
    time_t timer = lua_tointeger(L, 2);
    const char *locale = lua_tostring(L, 3);

    if (setlocale(LC_TIME, locale) == NULL)
        luaL_error(L, "Impossible to set the \"%s\" locale.", locale);

    tm_t *info = localtime(&timer);

    return format(L, pattern, info, lua_gettop(L)); // the date table has to be the last argument, period.
}

int l_mktime(lua_State *L)
{
    int lua_type;
    tm_t info;

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

    lua_type = lua_getfield(L, 1, "gmtoff");
    luaL_argcheck(L, lua_type == LUA_TNUMBER, 1, "Expected an integer for \"gmtoff\" [0-]");
    info.tm_gmtoff = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_type = lua_getfield(L, 1, "zone");
    luaL_argcheck(L, lua_type == LUA_TSTRING, 1, "Expected an integer for \"zone\" [string]");
    info.tm_zone = lua_tostring(L, -1);
    lua_pop(L, 1);

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

    tm_t *info = gm ? gmtime(&timer) : localtime(&timer);

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

    lua_pushinteger(L, info->tm_gmtoff);
    lua_setfield(L, -2, "gmtoff");

    lua_pushstring(L, info->tm_zone);
    lua_setfield(L, -2, "zone");

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
    lua_pushinteger(L, DATE);
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

int luaopen_libdatetimeformatter(lua_State *L)
{
    luaL_newlib(L, lib);

    return 1;
}
