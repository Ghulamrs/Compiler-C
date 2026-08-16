#ifndef TYPES_H
#define TYPES_H

// Struct with mixed numeric types
typedef struct {
    float f;
    double d;
    long l;
    long long ll;
    long double ld;
} NumSet;

// Union to reinterpret memory
typedef union {
    int i;
    float f;
    char bytes[4];
} IntFloatUnion;

// Function pointer demo
typedef void (*printer_t)(NumSet);

void print_numset(NumSet ns);

#endif
