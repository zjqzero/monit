/* Generated by re2c 1.3 on Sat Jun 20 14:58:47 2020 */
/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU Affero General Public License in all respects
 * for all of the code used other than OpenSSL.
 */


#include "Config.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>

#include "Str.h"
#include "system/System.h"
#include "system/Time.h"


/**
 * Implementation of the Time interface
 *
 * @author http://www.tildeslash.com/
 * @see http://www.mmonit.com/
 * @file
 */


/* ----------------------------------------------------------- Definitions */


#ifndef HAVE_TIMEGM
/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/* Counter the number of leap year in the range [0, y). The |y| is the
 year, including century (e.g., 2012) */
static int count_leap_year(int y)
{
        y -= 1;
        return y/4-y/100+y/400;
}


/* Returns nonzero if the |y| is the leap year. The |y| is the year,
 including century (e.g., 2012) */
static bool is_leap_year(int y)
{
        return y%4 == 0 && (y%100 != 0 || y%400 == 0);
}


/* The number of days before ith month begins */
static int daysum[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};


/* Based on the algorithm of Python 2.7 calendar.timegm. */
time_t timegm(struct tm *tm)
{
        int days;
        int num_leap_year;
        long long t;
        if(tm->tm_mon > 11) {
                return -1;
        }
        num_leap_year = count_leap_year(tm->tm_year + 1900) - count_leap_year(1970);
        days = (tm->tm_year - 70) * 365 +
        num_leap_year + daysum[tm->tm_mon] + tm->tm_mday-1;
        if(tm->tm_mon >= 2 && is_leap_year(tm->tm_year + 1900)) {
                ++days;
        }
        t = ((long long)days * 24 + tm->tm_hour) * 3600 + tm->tm_min * 60 + tm->tm_sec;
        if(sizeof(time_t) == 4) {
                if(t < INT_MIN || t > INT_MAX) {
                        return -1;
                }
        }
        return t;
}
#endif /* !HAVE_TIMEGM */


/* ----------------------------------------------------------------------- */


#if HAVE_STRUCT_TM_TM_GMTOFF
#define TM_GMTOFF tm_gmtoff
#else
#define TM_GMTOFF tm_wday
#endif

#define _i2a(i) (x[0] = ((i) / 10) + '0', x[1] = ((i) % 10) + '0')
#define _isValidDate ((tm.tm_mday < 32 && tm.tm_mday >= 1) && (tm.tm_mon < 12 && tm.tm_mon >= 0))
#define _isValidTime ((tm.tm_hour < 24 && tm.tm_hour >= 0) && (tm.tm_min < 60 && tm.tm_min >= 0) && (tm.tm_sec < 61 && tm.tm_sec >= 0))

#define TEST_RANGE(v, f, t) \
        do { \
                if (v < f || v > t) \
                        THROW(AssertException, "#v is outside the range (%d..%d)", f, t); \
        } while (0)
static const char days[] = "SunMonTueWedThuFriSat";
static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";


/* --------------------------------------------------------------- Private */


static inline int _a2i(const char *a, int l) {
        int n = 0;
        for (; *a && l--; a++)
                n = n * 10 + (*a) - '0';
        return n;
}


static inline int _m2i(const char m[static 3]) {
        char month[3] = {[0] = tolower(m[0]), [1] = tolower(m[1]), [2] = tolower(m[2])};
        static const char *months = "janfebmaraprmayjunjulaugsepoctnovdec";
        for (int i = 0; i < 34; i += 3) {
                if (memcmp(months + i, month, 3) == 0)
                        return i / 3;
        }
        return -1;
}


/* ----------------------------------------------------------------- Class */


time_t Time_toTimestamp(const char *s) {
        if (STR_DEF(s)) {
                struct tm t = {};
                if (Time_toDateTime(s, &t)) {
                        t.tm_year -= 1900;
                        time_t offset = t.TM_GMTOFF;
                        return timegm(&t) - offset;
                }
        }
        return 0;
}


struct tm *Time_toDateTime(const char *s, struct tm *t) {
        assert(t);
        assert(s);
        struct tm tm = {.tm_isdst = -1};
        bool have_date = false, have_time = false;
        const char *limit = s + strlen(s), *marker, *token, *cursor = s;
        while (true) {
                if (cursor >= limit) {
                        if (have_date || have_time) {
                                *(struct tm*)t = tm;
                                return t;
                        }
                        THROW(AssertException, "Invalid date or time");
                }
                token = cursor;
                
{
        unsigned char yych;
        unsigned int yyaccept = 0;
        yych = *cursor;
        switch (yych) {
        case '+':
        case '-':        goto yy4;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy5;
        default:
                if (limit <= cursor) {
                        goto yyeof;
                }
                goto yy2;
        }
yy2:
        ++cursor;
yy3:
        {
                        continue;
                 }
yy4:
        yyaccept = 0;
        yych = *(marker = ++cursor);
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy6;
        default:
                if (limit <= cursor) {
                }
                goto yy3;
        }
yy5:
        yyaccept = 0;
        yych = *(marker = ++cursor);
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy8;
        default:
                if (limit <= cursor) {
                }
                goto yy3;
        }
yy6:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy9;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy7:
        cursor = marker;
        switch (yyaccept) {
        case 0:         goto yy3;
        case 1:         goto yy10;
        case 2:         goto yy43;
        case 3:         goto yy49;
        default:        goto yy56;
        }
yy8:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy12;
        case ':':        goto yy13;
        default:
                if (limit <= cursor) {
                        goto yy7;
                }
                goto yy11;
        }
yy9:
        yyaccept = 1;
        yych = *(marker = ++cursor);
        switch (yych) {
        case '\n':        goto yy10;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy15;
        default:
                if (limit <= cursor) {
                        goto yy10;
                }
                goto yy14;
        }
yy10:
        { // Timezone: +-HH:MM, +-HH or +-HHMM is offset from UTC in seconds
                        if (have_time) { // Only set timezone if we have parsed time
                                tm.TM_GMTOFF = _a2i(token + 1, 2) * 3600;
                                if (isdigit(token[3]))
                                        tm.TM_GMTOFF += _a2i(token + 3, 2) * 60;
                                else if (isdigit(token[4]))
                                        tm.TM_GMTOFF += _a2i(token + 4, 2) * 60;
                                if (token[0] == '-')
                                        tm.TM_GMTOFF *= -1;
                        }
                        continue;
                 }
yy11:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy16;
        case 'A':
        case 'a':        goto yy17;
        case 'D':
        case 'd':        goto yy18;
        case 'F':
        case 'f':        goto yy19;
        case 'J':
        case 'j':        goto yy20;
        case 'M':
        case 'm':        goto yy21;
        case 'N':
        case 'n':        goto yy22;
        case 'O':
        case 'o':        goto yy23;
        case 'S':
        case 's':        goto yy24;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy12:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy25;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy13:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy26;
        case 'A':
        case 'a':        goto yy17;
        case 'D':
        case 'd':        goto yy18;
        case 'F':
        case 'f':        goto yy19;
        case 'J':
        case 'j':        goto yy20;
        case 'M':
        case 'm':        goto yy21;
        case 'N':
        case 'n':        goto yy22;
        case 'O':
        case 'o':        goto yy23;
        case 'S':
        case 's':        goto yy24;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy14:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy27;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy15:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy28;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy16:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy29;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy17:
        yych = *++cursor;
        switch (yych) {
        case 'P':
        case 'p':        goto yy30;
        case 'U':
        case 'u':        goto yy31;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy18:
        yych = *++cursor;
        switch (yych) {
        case 'E':
        case 'e':        goto yy32;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy19:
        yych = *++cursor;
        switch (yych) {
        case 'E':
        case 'e':        goto yy33;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy20:
        yych = *++cursor;
        switch (yych) {
        case 'A':
        case 'a':        goto yy34;
        case 'U':
        case 'u':        goto yy35;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy21:
        yych = *++cursor;
        switch (yych) {
        case 'A':
        case 'a':        goto yy36;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy22:
        yych = *++cursor;
        switch (yych) {
        case 'O':
        case 'o':        goto yy37;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy23:
        yych = *++cursor;
        switch (yych) {
        case 'C':
        case 'c':        goto yy38;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy24:
        yych = *++cursor;
        switch (yych) {
        case 'E':
        case 'e':        goto yy39;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy25:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy41;
        default:
                if (limit <= cursor) {
                        goto yy7;
                }
                goto yy40;
        }
yy26:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy42;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy27:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy44;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy28:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy44;
        default:
                if (limit <= cursor) {
                }
                goto yy10;
        }
yy29:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy7;
        default:
                if (limit <= cursor) {
                        goto yy7;
                }
                goto yy45;
        }
yy30:
        yych = *++cursor;
        switch (yych) {
        case 'R':
        case 'r':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy31:
        yych = *++cursor;
        switch (yych) {
        case 'G':
        case 'g':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy32:
        yych = *++cursor;
        switch (yych) {
        case 'C':
        case 'c':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy33:
        yych = *++cursor;
        switch (yych) {
        case 'B':
        case 'b':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy34:
        yych = *++cursor;
        switch (yych) {
        case 'N':
        case 'n':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy35:
        yych = *++cursor;
        switch (yych) {
        case 'L':
        case 'N':
        case 'l':
        case 'n':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy36:
        yych = *++cursor;
        switch (yych) {
        case 'R':
        case 'Y':
        case 'r':
        case 'y':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy37:
        yych = *++cursor;
        switch (yych) {
        case 'V':
        case 'v':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy38:
        yych = *++cursor;
        switch (yych) {
        case 'T':
        case 't':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy39:
        yych = *++cursor;
        switch (yych) {
        case 'P':
        case 'p':        goto yy46;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy40:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy47;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy41:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy48;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy42:
        yyaccept = 2;
        yych = *(marker = ++cursor);
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy43;
        default:
                if (limit <= cursor) {
                        goto yy43;
                }
                goto yy45;
        }
yy43:
        { // Time: HH:MM
                        tm.tm_hour = _a2i(token, 2);
                        tm.tm_min  = _a2i(token + 3, 2);
                        tm.tm_sec  = 0;
                        have_time  = _isValidTime;
                        continue;
                 }
yy44:
        ++cursor;
        goto yy10;
yy45:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy50;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy46:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy7;
        default:
                if (limit <= cursor) {
                        goto yy7;
                }
                goto yy51;
        }
yy47:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy52;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy48:
        yyaccept = 3;
        yych = *(marker = ++cursor);
        switch (yych) {
        case ',':
        case '.':        goto yy53;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy54;
        default:
                if (limit <= cursor) {
                }
                goto yy49;
        }
yy49:
        { // Compressed Time: HHMMSS
                        tm.tm_hour = _a2i(token, 2);
                        tm.tm_min  = _a2i(token + 2, 2);
                        tm.tm_sec  = _a2i(token + 4, 2);
                        have_time  = _isValidTime;
                        continue;
                 }
yy50:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy55;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy51:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy57;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy52:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy7;
        default:
                if (limit <= cursor) {
                        goto yy7;
                }
                goto yy58;
        }
yy53:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy59;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy54:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy61;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy55:
        yyaccept = 4;
        yych = *(marker = ++cursor);
        switch (yych) {
        case ',':
        case '.':        goto yy63;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy64;
        default:
                if (limit <= cursor) {
                }
                goto yy56;
        }
yy56:
        { // Time: HH:MM:SS
                        tm.tm_hour = _a2i(token, 2);
                        tm.tm_min  = _a2i(token + 3, 2);
                        tm.tm_sec  = _a2i(token + 6, 2);
                        have_time  = _isValidTime;
                        continue;
                 }
yy57:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy65;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy58:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy66;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy59:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy59;
        default:
                if (limit <= cursor) {
                }
                goto yy49;
        }
yy61:
        ++cursor;
        { // Compressed Date: YYYYMMDD
                        tm.tm_year = _a2i(token, 4);
                        tm.tm_mon  = _a2i(token + 4, 2) - 1;
                        tm.tm_mday = _a2i(token + 6, 2);
                        have_date  = _isValidDate;
                        continue;
                 }
yy63:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy67;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy64:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy69;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy65:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy71;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy66:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy72;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy67:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy67;
        default:
                if (limit <= cursor) {
                }
                goto yy56;
        }
yy69:
        ++cursor;
        { // Date: dd/mm/yyyy
                        tm.tm_mday = _a2i(token, 2);
                        tm.tm_mon  = _a2i(token + 3, 2) - 1;
                        tm.tm_year = _a2i(token + 6, 4);
                        have_date  = _isValidDate;
                        continue;
                 }
yy71:
        yych = *++cursor;
        switch (yych) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':        goto yy74;
        default:
                if (limit <= cursor) {
                }
                goto yy7;
        }
yy72:
        ++cursor;
        { // Date: YYYY-MM-DD
                        tm.tm_year = _a2i(token, 4);
                        tm.tm_mon  = _a2i(token + 5, 2) - 1;
                        tm.tm_mday = _a2i(token + 8, 2);
                        have_date  = _isValidDate;
                        continue;
                 }
yy74:
        ++cursor;
        { // Date: Parse date part of RFC 7231 IMF-fixdate (HTTP date), e.g. Sun, 06 Nov 1994 08:49:37 GMT
                        tm.tm_mday = _a2i(token, 2);
                        tm.tm_mon  = _m2i(token + 3);
                        tm.tm_year = _a2i(token + 7, 4);
                        have_date  = _isValidDate;
                        continue;
                 }
yyeof:
        { // EOF
                        THROW(AssertException, "Invalid date or time");
                 }
}

        }
        return NULL;
}


time_t Time_build(int year, int month, int day, int hour, int min, int sec) {
        TEST_RANGE(year, 1970, 2037);
        TEST_RANGE(month, 1, 12);
        TEST_RANGE(day, 1, 31);
        TEST_RANGE(hour, 0, 23);
        TEST_RANGE(min, 0, 59);
        TEST_RANGE(sec, 0, 61);
        return mktime(&(struct tm) {
                .tm_isdst = -1,
                .tm_year = (year - 1900),
                .tm_mon = (month - 1),
                .tm_mday = day,
                .tm_hour = hour,
                .tm_min  = min,
                .tm_sec  = sec
        });
}


time_t Time_now(void) {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0)
                THROW(AssertException, "%s", System_getLastError());
	return t.tv_sec;
}


time_t Time_monotonic(void) {
	struct timespec t;
#ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &t) != 0)
#else
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
#endif
                THROW(AssertException, "%s", System_getLastError());
	return t.tv_sec;
}


long long Time_milli(void) {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0)
                THROW(AssertException, "%s", System_getLastError());
	return (long long)t.tv_sec * 1000  +  (long long)t.tv_usec / 1000;
}


long long Time_micro(void) {
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0)
                THROW(AssertException, "%s", System_getLastError());
	return (long long)t.tv_sec * 1000000  +  (long long)t.tv_usec;
}


int Time_seconds(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return tm.tm_sec;
}


int Time_minutes(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return tm.tm_min;
}


int Time_hour(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return tm.tm_hour;
}


int Time_weekday(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return tm.tm_wday;
}


int Time_day(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return tm.tm_mday;
}


int Time_month(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return (tm.tm_mon + 1);
}


int Time_year(time_t time) {
        struct tm tm;
        localtime_r(&time, &tm);
        return (tm.tm_year + 1900);
}


char *Time_string(time_t time, char result[static 26]) {
        if (result) {
                char x[2];
                struct tm ts;
                localtime_r((const time_t *)&time, &ts);
                memcpy(result, "aaa, xx aaa xxxx xx:xx:xx\0", 26);
                /*              0    5  8   1214 17 20 2326 */
                memcpy(result, days + 3 * ts.tm_wday, 3);
                _i2a(ts.tm_mday);
                result[5] = x[0];
                result[6] = x[1];
                memcpy(result + 8, months + 3 * ts.tm_mon, 3);
                _i2a((ts.tm_year + 1900) / 100);
                result[12] = x[0];
                result[13] = x[1];
                _i2a((ts.tm_year + 1900) % 100);
                result[14] = x[0];
                result[15] = x[1];
                _i2a(ts.tm_hour);
                result[17] = x[0];
                result[18] = x[1];
                _i2a(ts.tm_min);
                result[20] = x[0];
                result[21] = x[1];
                _i2a(ts.tm_sec);
                result[23] = x[0];
                result[24] = x[1];
        }
	return result;
}


char *Time_gmtstring(time_t time, char result[static 30]) {
        if (result) {
                char x[2];
                struct tm ts;
                gmtime_r(&time, &ts);
                memcpy(result, "aaa, xx aaa xxxx xx:xx:xx GMT\0", 30);
                /*              0    5  8   1214 17 20 23    29 */
                memcpy(result, days + 3 * ts.tm_wday, 3);
                _i2a(ts.tm_mday);
                result[5] = x[0];
                result[6] = x[1];
                memcpy(result + 8, months + 3 * ts.tm_mon, 3);
                _i2a((ts.tm_year + 1900) / 100);
                result[12] = x[0];
                result[13] = x[1];
                _i2a((ts.tm_year + 1900) % 100);
                result[14] = x[0];
                result[15] = x[1];
                _i2a(ts.tm_hour);
                result[17] = x[0];
                result[18] = x[1];
                _i2a(ts.tm_min);
                result[20] = x[0];
                result[21] = x[1];
                _i2a(ts.tm_sec);
                result[23] = x[0];
                result[24] = x[1];
        }
	return result;
}


char *Time_fmt(char *result, int size, const char *format, time_t time) {
        struct tm tm;
        assert(result);
        assert(format);
        localtime_r((const time_t *)&time, &tm);
        if (strftime(result, size, format, &tm) == 0)
                *result = 0;
        return result;
}


char *Time_uptime(time_t sec, char result[static 24]) {
        // Write max 24 bytes to result
        if (result) {
                int n = 0;
                time_t r = 0;
                result[0] = 0;
                if (sec > 0) {
                        if ((r = sec/86400) > 0) {
                                n = snprintf(result, 24, "%lldd", (long long)r);
                                sec -= r * 86400;
                        }
                        if ((r = sec/3600) > 0) {
                                n += snprintf(result + n, (24 - n), "%s%lldh", n ? ", " : "", (long long)r);
                                sec -= r * 3600;
                        }
                        r = sec/60;
                        snprintf(result + n, (24 - n), "%s%lldm", n ? ", " : "", (long long)r);
                }
        }
        return result;
}


/*
 cron string is on format "minute hour day month wday"
 where fields may have a numeric type, an asterix, a
 sequence of numbers or a range
 */
int Time_incron(const char *cron, time_t time) {
        assert(cron);
#undef YYCURSOR
#undef YYLIMIT
#undef YYMARKER
#define YYCURSOR cron
#define YYLIMIT  end
#define YYMARKER m
#define YYTOKEN  t
        const char *m;
        const char *t;
        const char *end = cron + strlen(cron);
        int n = 0;
        int found = 0;
        int fields[] = {Time_minutes(time), Time_hour(time), Time_day(time), Time_month(time), Time_weekday(time)};
parse:
        if (YYCURSOR >= YYLIMIT)
                return found == 5;
        YYTOKEN = YYCURSOR;
        
{
	unsigned char yych;
	yych = *YYCURSOR;
	switch (yych) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':	goto yy55;
	case '*':	goto yy57;
	case ',':	goto yy59;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':	goto yy61;
	default:	goto yy53;
	}
yy53:
	++YYCURSOR;
	{
                return false;
        }
yy55:
	++YYCURSOR;
	{
                goto parse;
        }
yy57:
	++YYCURSOR;
	{
                n++;
                found++;
                goto parse;
        }
yy59:
	++YYCURSOR;
	{
                n--; // backtrack on fields advance
                assert(n < 5 && n >= 0);
                goto parse;
        }
yy61:
	yych = *(YYMARKER = ++YYCURSOR);
	switch (yych) {
	case '-':	goto yy64;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':	goto yy61;
	default:	goto yy63;
	}
yy63:
	{
                if (fields[n] == Str_parseInt(YYTOKEN))
                        found++;
                n++;
                goto parse;
        }
yy64:
	yych = *++YYCURSOR;
	switch (yych) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':	goto yy66;
	default:	goto yy65;
	}
yy65:
	YYCURSOR = YYMARKER;
	goto yy63;
yy66:
	yych = *++YYCURSOR;
	switch (yych) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':	goto yy66;
	default:	goto yy68;
	}
yy68:
	{
                int from = Str_parseInt(YYTOKEN);
                int to = Str_parseInt(strchr(YYTOKEN, '-') + 1);
                if ((fields[n] <= to) && (fields[n] >= from))
                        found++;
                n++;
                goto parse;
        }
}
        return found == 5;
}


void Time_usleep(long u) {
#ifdef NETBSD
        // usleep is broken on NetBSD (at least in version 5.1)
        struct timespec t = {u / 1000000, (u % 1000000) * 1000};
        nanosleep(&t, NULL);
#else
        usleep((useconds_t)u);
#endif
}

