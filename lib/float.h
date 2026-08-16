// float.h - the shape of float and double.
//
// Unlike <limits.h> this file has no #ifdef in it, and that is worth stating
// rather than leaving to be noticed: all three targets use IEEE 754 binary32
// and binary64, so every value below is the same on every one of them. It was
// checked on all three before being written that way - macOS, Linux and the
// UCRT agree on all of it.
//
// long double is absent because this compiler does not have the type. It
// refuses it by name rather than quietly treating it as double, so the LDBL_
// macros would describe something that cannot be declared.
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

#endif
