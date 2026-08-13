// heavy.c - a long program, here to be compiled rather than admired.
//
// It exists to put weight on the front end: the lexer and parser are 75 per
// cent of this compiler's time, and 361 test programs of ten lines each cannot
// show that. Everything here runs in well under a second - what is being
// measured is the compile, not the answer.
//
// Every function is reachable from heavy_main, and the total it prints is
// checked, so this cannot rot into a file that compiles and means nothing.
#include <stdio.h>
#include <string.h>
#include "examples.h"

static int primes_below(int n) {
    static char sieve[2048];
    int i, j, count;

    if (n > 2048) n = 2048;
    for (i = 0; i < n; i = i + 1) sieve[i] = 1;
    sieve[0] = 0;
    if (n > 1) sieve[1] = 0;
    for (i = 2; i * i < n; i = i + 1)
        if (sieve[i])
            for (j = i * i; j < n; j = j + i) sieve[j] = 0;
    count = 0;
    for (i = 0; i < n; i = i + 1) if (sieve[i]) count = count + 1;
    return count;
}

static void bubble(int *a, int n) {
    int i, j, t;

    for (i = 0; i < n - 1; i = i + 1)
        for (j = 0; j < n - 1 - i; j = j + 1)
            if (a[j] > a[j + 1]) { t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; }
}

static long fib(int n) {
    long a = 0, b = 1, t;
    int i;

    for (i = 0; i < n; i = i + 1) { t = a + b; a = b; b = t; }
    return a;
}

static int gcd(int a, int b) {
    while (b != 0) { int t = b; b = a % b; a = t; }
    return a;
}

static double poly(double x, int terms) {
    double sum = 0.0;
    double p = 1.0;
    int i;

    for (i = 0; i < terms; i = i + 1) { sum = sum + p / (i + 1); p = p * x; }
    return sum;
}
static int step_1(int x) {
    int a = x + 1;
    int b = a * 3;
    int c = b - 3;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_2(int x) {
    int a = x + 2;
    int b = a * 4;
    int c = b - 6;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_3(int x) {
    int a = x + 3;
    int b = a * 5;
    int c = b - 9;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_4(int x) {
    int a = x + 4;
    int b = a * 6;
    int c = b - 12;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_5(int x) {
    int a = x + 5;
    int b = a * 7;
    int c = b - 15;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_6(int x) {
    int a = x + 6;
    int b = a * 8;
    int c = b - 18;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_7(int x) {
    int a = x + 7;
    int b = a * 2;
    int c = b - 21;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_8(int x) {
    int a = x + 8;
    int b = a * 3;
    int c = b - 24;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_9(int x) {
    int a = x + 9;
    int b = a * 4;
    int c = b - 27;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_10(int x) {
    int a = x + 10;
    int b = a * 5;
    int c = b - 30;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_11(int x) {
    int a = x + 11;
    int b = a * 6;
    int c = b - 33;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_12(int x) {
    int a = x + 12;
    int b = a * 7;
    int c = b - 36;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_13(int x) {
    int a = x + 13;
    int b = a * 8;
    int c = b - 39;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_14(int x) {
    int a = x + 14;
    int b = a * 2;
    int c = b - 42;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_15(int x) {
    int a = x + 15;
    int b = a * 3;
    int c = b - 45;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_16(int x) {
    int a = x + 16;
    int b = a * 4;
    int c = b - 48;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_17(int x) {
    int a = x + 17;
    int b = a * 5;
    int c = b - 51;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_18(int x) {
    int a = x + 18;
    int b = a * 6;
    int c = b - 54;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_19(int x) {
    int a = x + 19;
    int b = a * 7;
    int c = b - 57;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_20(int x) {
    int a = x + 20;
    int b = a * 8;
    int c = b - 60;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_21(int x) {
    int a = x + 21;
    int b = a * 2;
    int c = b - 63;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_22(int x) {
    int a = x + 22;
    int b = a * 3;
    int c = b - 66;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_23(int x) {
    int a = x + 23;
    int b = a * 4;
    int c = b - 69;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_24(int x) {
    int a = x + 24;
    int b = a * 5;
    int c = b - 72;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_25(int x) {
    int a = x + 25;
    int b = a * 6;
    int c = b - 75;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_26(int x) {
    int a = x + 26;
    int b = a * 7;
    int c = b - 78;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_27(int x) {
    int a = x + 27;
    int b = a * 8;
    int c = b - 81;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_28(int x) {
    int a = x + 28;
    int b = a * 2;
    int c = b - 84;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_29(int x) {
    int a = x + 29;
    int b = a * 3;
    int c = b - 87;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_30(int x) {
    int a = x + 30;
    int b = a * 4;
    int c = b - 90;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_31(int x) {
    int a = x + 31;
    int b = a * 5;
    int c = b - 93;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_32(int x) {
    int a = x + 32;
    int b = a * 6;
    int c = b - 96;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_33(int x) {
    int a = x + 33;
    int b = a * 7;
    int c = b - 99;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_34(int x) {
    int a = x + 34;
    int b = a * 8;
    int c = b - 102;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_35(int x) {
    int a = x + 35;
    int b = a * 2;
    int c = b - 105;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_36(int x) {
    int a = x + 36;
    int b = a * 3;
    int c = b - 108;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_37(int x) {
    int a = x + 37;
    int b = a * 4;
    int c = b - 111;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_38(int x) {
    int a = x + 38;
    int b = a * 5;
    int c = b - 114;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_39(int x) {
    int a = x + 39;
    int b = a * 6;
    int c = b - 117;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_40(int x) {
    int a = x + 40;
    int b = a * 7;
    int c = b - 120;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_41(int x) {
    int a = x + 41;
    int b = a * 8;
    int c = b - 123;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_42(int x) {
    int a = x + 42;
    int b = a * 2;
    int c = b - 126;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_43(int x) {
    int a = x + 43;
    int b = a * 3;
    int c = b - 129;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_44(int x) {
    int a = x + 44;
    int b = a * 4;
    int c = b - 132;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_45(int x) {
    int a = x + 45;
    int b = a * 5;
    int c = b - 135;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_46(int x) {
    int a = x + 46;
    int b = a * 6;
    int c = b - 138;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_47(int x) {
    int a = x + 47;
    int b = a * 7;
    int c = b - 141;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_48(int x) {
    int a = x + 48;
    int b = a * 8;
    int c = b - 144;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_49(int x) {
    int a = x + 49;
    int b = a * 2;
    int c = b - 147;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_50(int x) {
    int a = x + 50;
    int b = a * 3;
    int c = b - 150;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_51(int x) {
    int a = x + 51;
    int b = a * 4;
    int c = b - 153;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_52(int x) {
    int a = x + 52;
    int b = a * 5;
    int c = b - 156;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_53(int x) {
    int a = x + 53;
    int b = a * 6;
    int c = b - 159;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_54(int x) {
    int a = x + 54;
    int b = a * 7;
    int c = b - 162;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_55(int x) {
    int a = x + 55;
    int b = a * 8;
    int c = b - 165;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_56(int x) {
    int a = x + 56;
    int b = a * 2;
    int c = b - 168;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_57(int x) {
    int a = x + 57;
    int b = a * 3;
    int c = b - 171;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_58(int x) {
    int a = x + 58;
    int b = a * 4;
    int c = b - 174;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_59(int x) {
    int a = x + 59;
    int b = a * 5;
    int c = b - 177;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_60(int x) {
    int a = x + 60;
    int b = a * 6;
    int c = b - 180;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_61(int x) {
    int a = x + 61;
    int b = a * 7;
    int c = b - 183;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_62(int x) {
    int a = x + 62;
    int b = a * 8;
    int c = b - 186;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_63(int x) {
    int a = x + 63;
    int b = a * 2;
    int c = b - 189;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_64(int x) {
    int a = x + 64;
    int b = a * 3;
    int c = b - 192;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_65(int x) {
    int a = x + 65;
    int b = a * 4;
    int c = b - 195;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_66(int x) {
    int a = x + 66;
    int b = a * 5;
    int c = b - 198;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_67(int x) {
    int a = x + 67;
    int b = a * 6;
    int c = b - 201;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_68(int x) {
    int a = x + 68;
    int b = a * 7;
    int c = b - 204;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_69(int x) {
    int a = x + 69;
    int b = a * 8;
    int c = b - 207;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_70(int x) {
    int a = x + 70;
    int b = a * 2;
    int c = b - 210;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_71(int x) {
    int a = x + 71;
    int b = a * 3;
    int c = b - 213;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_72(int x) {
    int a = x + 72;
    int b = a * 4;
    int c = b - 216;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_73(int x) {
    int a = x + 73;
    int b = a * 5;
    int c = b - 219;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_74(int x) {
    int a = x + 74;
    int b = a * 6;
    int c = b - 222;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_75(int x) {
    int a = x + 75;
    int b = a * 7;
    int c = b - 225;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_76(int x) {
    int a = x + 76;
    int b = a * 8;
    int c = b - 228;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_77(int x) {
    int a = x + 77;
    int b = a * 2;
    int c = b - 231;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_78(int x) {
    int a = x + 78;
    int b = a * 3;
    int c = b - 234;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_79(int x) {
    int a = x + 79;
    int b = a * 4;
    int c = b - 237;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_80(int x) {
    int a = x + 80;
    int b = a * 5;
    int c = b - 240;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_81(int x) {
    int a = x + 81;
    int b = a * 6;
    int c = b - 243;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_82(int x) {
    int a = x + 82;
    int b = a * 7;
    int c = b - 246;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_83(int x) {
    int a = x + 83;
    int b = a * 8;
    int c = b - 249;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_84(int x) {
    int a = x + 84;
    int b = a * 2;
    int c = b - 252;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_85(int x) {
    int a = x + 85;
    int b = a * 3;
    int c = b - 255;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_86(int x) {
    int a = x + 86;
    int b = a * 4;
    int c = b - 258;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_87(int x) {
    int a = x + 87;
    int b = a * 5;
    int c = b - 261;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_88(int x) {
    int a = x + 88;
    int b = a * 6;
    int c = b - 264;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_89(int x) {
    int a = x + 89;
    int b = a * 7;
    int c = b - 267;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_90(int x) {
    int a = x + 90;
    int b = a * 8;
    int c = b - 270;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_91(int x) {
    int a = x + 91;
    int b = a * 2;
    int c = b - 273;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_92(int x) {
    int a = x + 92;
    int b = a * 3;
    int c = b - 276;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_93(int x) {
    int a = x + 93;
    int b = a * 4;
    int c = b - 279;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_94(int x) {
    int a = x + 94;
    int b = a * 5;
    int c = b - 282;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_95(int x) {
    int a = x + 95;
    int b = a * 6;
    int c = b - 285;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_96(int x) {
    int a = x + 96;
    int b = a * 7;
    int c = b - 288;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_97(int x) {
    int a = x + 97;
    int b = a * 8;
    int c = b - 291;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_98(int x) {
    int a = x + 98;
    int b = a * 2;
    int c = b - 294;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_99(int x) {
    int a = x + 99;
    int b = a * 3;
    int c = b - 297;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_100(int x) {
    int a = x + 100;
    int b = a * 4;
    int c = b - 300;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_101(int x) {
    int a = x + 101;
    int b = a * 5;
    int c = b - 303;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_102(int x) {
    int a = x + 102;
    int b = a * 6;
    int c = b - 306;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_103(int x) {
    int a = x + 103;
    int b = a * 7;
    int c = b - 309;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_104(int x) {
    int a = x + 104;
    int b = a * 8;
    int c = b - 312;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_105(int x) {
    int a = x + 105;
    int b = a * 2;
    int c = b - 315;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_106(int x) {
    int a = x + 106;
    int b = a * 3;
    int c = b - 318;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_107(int x) {
    int a = x + 107;
    int b = a * 4;
    int c = b - 321;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_108(int x) {
    int a = x + 108;
    int b = a * 5;
    int c = b - 324;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_109(int x) {
    int a = x + 109;
    int b = a * 6;
    int c = b - 327;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_110(int x) {
    int a = x + 110;
    int b = a * 7;
    int c = b - 330;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_111(int x) {
    int a = x + 111;
    int b = a * 8;
    int c = b - 333;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_112(int x) {
    int a = x + 112;
    int b = a * 2;
    int c = b - 336;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_113(int x) {
    int a = x + 113;
    int b = a * 3;
    int c = b - 339;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_114(int x) {
    int a = x + 114;
    int b = a * 4;
    int c = b - 342;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_115(int x) {
    int a = x + 115;
    int b = a * 5;
    int c = b - 345;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_116(int x) {
    int a = x + 116;
    int b = a * 6;
    int c = b - 348;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_117(int x) {
    int a = x + 117;
    int b = a * 7;
    int c = b - 351;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_118(int x) {
    int a = x + 118;
    int b = a * 8;
    int c = b - 354;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_119(int x) {
    int a = x + 119;
    int b = a * 2;
    int c = b - 357;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}
static int step_120(int x) {
    int a = x + 120;
    int b = a * 3;
    int c = b - 360;

    if (c % 2 == 0) c = c / 2;
    else            c = c * 3 + 1;
    return c % 1000;
}

static int run_steps(int seed) {
    int total = 0;
    total = total + step_1(seed + 1);
    total = total + step_2(seed + 2);
    total = total + step_3(seed + 3);
    total = total + step_4(seed + 4);
    total = total + step_5(seed + 5);
    total = total + step_6(seed + 6);
    total = total + step_7(seed + 7);
    total = total + step_8(seed + 8);
    total = total + step_9(seed + 9);
    total = total + step_10(seed + 10);
    total = total + step_11(seed + 11);
    total = total + step_12(seed + 12);
    total = total + step_13(seed + 13);
    total = total + step_14(seed + 14);
    total = total + step_15(seed + 15);
    total = total + step_16(seed + 16);
    total = total + step_17(seed + 17);
    total = total + step_18(seed + 18);
    total = total + step_19(seed + 19);
    total = total + step_20(seed + 20);
    total = total + step_21(seed + 21);
    total = total + step_22(seed + 22);
    total = total + step_23(seed + 23);
    total = total + step_24(seed + 24);
    total = total + step_25(seed + 25);
    total = total + step_26(seed + 26);
    total = total + step_27(seed + 27);
    total = total + step_28(seed + 28);
    total = total + step_29(seed + 29);
    total = total + step_30(seed + 30);
    total = total + step_31(seed + 31);
    total = total + step_32(seed + 32);
    total = total + step_33(seed + 33);
    total = total + step_34(seed + 34);
    total = total + step_35(seed + 35);
    total = total + step_36(seed + 36);
    total = total + step_37(seed + 37);
    total = total + step_38(seed + 38);
    total = total + step_39(seed + 39);
    total = total + step_40(seed + 40);
    total = total + step_41(seed + 41);
    total = total + step_42(seed + 42);
    total = total + step_43(seed + 43);
    total = total + step_44(seed + 44);
    total = total + step_45(seed + 45);
    total = total + step_46(seed + 46);
    total = total + step_47(seed + 47);
    total = total + step_48(seed + 48);
    total = total + step_49(seed + 49);
    total = total + step_50(seed + 50);
    total = total + step_51(seed + 51);
    total = total + step_52(seed + 52);
    total = total + step_53(seed + 53);
    total = total + step_54(seed + 54);
    total = total + step_55(seed + 55);
    total = total + step_56(seed + 56);
    total = total + step_57(seed + 57);
    total = total + step_58(seed + 58);
    total = total + step_59(seed + 59);
    total = total + step_60(seed + 60);
    total = total + step_61(seed + 61);
    total = total + step_62(seed + 62);
    total = total + step_63(seed + 63);
    total = total + step_64(seed + 64);
    total = total + step_65(seed + 65);
    total = total + step_66(seed + 66);
    total = total + step_67(seed + 67);
    total = total + step_68(seed + 68);
    total = total + step_69(seed + 69);
    total = total + step_70(seed + 70);
    total = total + step_71(seed + 71);
    total = total + step_72(seed + 72);
    total = total + step_73(seed + 73);
    total = total + step_74(seed + 74);
    total = total + step_75(seed + 75);
    total = total + step_76(seed + 76);
    total = total + step_77(seed + 77);
    total = total + step_78(seed + 78);
    total = total + step_79(seed + 79);
    total = total + step_80(seed + 80);
    total = total + step_81(seed + 81);
    total = total + step_82(seed + 82);
    total = total + step_83(seed + 83);
    total = total + step_84(seed + 84);
    total = total + step_85(seed + 85);
    total = total + step_86(seed + 86);
    total = total + step_87(seed + 87);
    total = total + step_88(seed + 88);
    total = total + step_89(seed + 89);
    total = total + step_90(seed + 90);
    total = total + step_91(seed + 91);
    total = total + step_92(seed + 92);
    total = total + step_93(seed + 93);
    total = total + step_94(seed + 94);
    total = total + step_95(seed + 95);
    total = total + step_96(seed + 96);
    total = total + step_97(seed + 97);
    total = total + step_98(seed + 98);
    total = total + step_99(seed + 99);
    total = total + step_100(seed + 100);
    total = total + step_101(seed + 101);
    total = total + step_102(seed + 102);
    total = total + step_103(seed + 103);
    total = total + step_104(seed + 104);
    total = total + step_105(seed + 105);
    total = total + step_106(seed + 106);
    total = total + step_107(seed + 107);
    total = total + step_108(seed + 108);
    total = total + step_109(seed + 109);
    total = total + step_110(seed + 110);
    total = total + step_111(seed + 111);
    total = total + step_112(seed + 112);
    total = total + step_113(seed + 113);
    total = total + step_114(seed + 114);
    total = total + step_115(seed + 115);
    total = total + step_116(seed + 116);
    total = total + step_117(seed + 117);
    total = total + step_118(seed + 118);
    total = total + step_119(seed + 119);
    total = total + step_120(seed + 120);
    return total % 100000;
}

struct Shape { int kind; double a; double b; };

static double area(struct Shape *s) {
    if (s->kind == 0) return s->a * s->b;
    if (s->kind == 1) return 3.14159265358979 * s->a * s->a;
    return s->a * s->b / 2.0;
}

int heavy_main(void) {
    int v[64];
    struct Shape shapes[3];
    char text[64];
    int i, checks;

    printf("[heavy]\n");

    printf("  primes   : %d below 2048\n", primes_below(2048));

    for (i = 0; i < 64; i = i + 1) v[i] = (i * 37) % 101;
    bubble(v, 64);
    printf("  sorted   : %d %d %d\n", v[0], v[31], v[63]);

    printf("  fib(60)  : %ld\n", fib(60));
    printf("  gcd      : %d %d\n", gcd(1071, 462), gcd(17, 5));
    printf("  poly     : %.6f\n", poly(0.5, 20));
    printf("  steps    : %d\n", run_steps(3));

    shapes[0].kind = 0; shapes[0].a = 3.0; shapes[0].b = 4.0;
    shapes[1].kind = 1; shapes[1].a = 2.0; shapes[1].b = 0.0;
    shapes[2].kind = 2; shapes[2].a = 6.0; shapes[2].b = 5.0;
    printf("  areas    : %.2f %.4f %.2f\n", area(&shapes[0]), area(&shapes[1]),
           area(&shapes[2]));

    strcpy(text, "the quick brown fox");
    printf("  text     : %s (%lu)\n", text, strlen(text));

    checks = 8;
    return checks;
}
