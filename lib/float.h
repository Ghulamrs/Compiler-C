// float.h - the shape of float and double.
//
// The float and double halves have no #ifdef in them, and that is worth
// stating rather than leaving to be noticed: all three targets use IEEE 754
// binary32 and binary64, so every FLT_ and DBL_ value below is the same on
// every one of them. It was checked on all three - macOS, Linux and the UCRT
// agree on all of it.
//
// **The LDBL_ half is where they part company**, and it is the only place in
// this header a target has to be asked. System V gives 'long double' x87's
// 80-bit extended format, with a 64-bit significand and an exponent reaching
// 2^16384. Apple's arm64 and the UCRT both make long double another name for
// double - the same processor as the Linux target, in Windows' case, choosing
// not to use the 80-bit hardware. So LDBL_ equals DBL_ on two of the three,
// and C90 is satisfied either way: it asks only that long double be at least
// as wide as double, never that it be wider.
#ifndef _CC1_FLOAT_H
#define _CC1_FLOAT_H

// The radix, and how rounding is done. FLT_ROUNDS is 1 for round-to-nearest,
// which is what all three of these are set to when a program starts.
#define FLT_RADIX       2
#define FLT_ROUNDS      1

// Base-2 digits in the significand, base-10 digits that survive a round trip.
#define FLT_MANT_DIG    24
#define DBL_MANT_DIG    53
#define FLT_DIG         6
#define DBL_DIG         15

// The exponent range, first in the radix and then in decimal.
#define FLT_MIN_EXP     (-125)
#define DBL_MIN_EXP     (-1021)
#define FLT_MAX_EXP     128
#define DBL_MAX_EXP     1024
#define FLT_MIN_10_EXP  (-37)
#define DBL_MIN_10_EXP  (-307)
#define FLT_MAX_10_EXP  38
#define DBL_MAX_10_EXP  308

// The largest and smallest normalised magnitudes, and the gap between 1.0 and
// the next representable value above it - which is what a tolerance in a
// floating comparison is usually reaching for.
//
// Written to more digits than the type can hold so each rounds to the nearest
// representable value rather than to whatever a shorter literal reaches.
#define FLT_MAX         3.40282346638528859812e+38F
#define DBL_MAX         1.79769313486231570815e+308
#define FLT_MIN         1.17549435082228750797e-38F
#define DBL_MIN         2.22507385850720138309e-308
#define FLT_EPSILON     1.19209289550781250000e-7F
#define DBL_EPSILON     2.22044604925031308085e-16

// long double. x87's 80-bit format on System V, and double everywhere else.
#if defined(_WIN32) || defined(__APPLE__)

#define LDBL_MANT_DIG   53
#define LDBL_DIG        15
#define LDBL_MIN_EXP    (-1021)
#define LDBL_MAX_EXP    1024
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_10_EXP 308
#define LDBL_MAX        1.79769313486231570815e+308L
#define LDBL_MIN        2.22507385850720138309e-308L
#define LDBL_EPSILON    2.22044604925031308085e-16L

#else

// 64 bits of significand rather than 53, because x87 stores the leading one
// explicitly instead of implying it - which is why the epsilon here is 2^-63
// and not 2^-64, and why 1.0L/3.0L prints four more correct digits than a
// double can carry.
#define LDBL_MANT_DIG   64
#define LDBL_DIG        18
#define LDBL_MIN_EXP    (-16381)
#define LDBL_MAX_EXP    16384
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_10_EXP 4932
#define LDBL_MAX        1.18973149535723176502e+4932L
#define LDBL_MIN        3.36210314311209350626e-4932L
#define LDBL_EPSILON    1.08420217248550443401e-19L

#endif

#endif
