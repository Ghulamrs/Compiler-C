// main.cpp - nothing but a way in.
//
// Everything the compiler does lives behind Driver, so that a second way in -
// a test harness, a build tool, a process that compiles several files at once -
// needs no part of this file.
#include "Driver.h"

int main(int argc, char **argv) {
    Driver driver;
    return driver.run(argc, argv);
}
