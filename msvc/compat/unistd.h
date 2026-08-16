/* <unistd.h> for MSVC, which does not have one.
 *
 * This exists so that the compiler's own source needs no change to build here.
 * src/Driver.cpp includes <unistd.h> and uses exactly one thing out of it -
 * getpid, to keep two cc1 runs in one directory from choosing the same
 * temporary name - and Windows has that function under another name in a
 * different header.
 *
 * So this directory goes on the include path ahead of everything else, this
 * file answers the include, and nothing in src/ knows the difference. Three
 * lines of shim against a platform #ifdef in a file that is otherwise about
 * compiling C: the shim is the smaller change, and it keeps the port in the
 * project that wants it rather than in the compiler that does not.
 *
 * If cc1 ever wants a second thing from POSIX, this is where to notice it -
 * an include that resolves here rather than to the real header is a decision,
 * and it should stay a visible one.
 */
#ifndef CC1_MSVC_COMPAT_UNISTD_H
#define CC1_MSVC_COMPAT_UNISTD_H

#include <process.h>

/* _getpid is the same function under Microsoft's leading-underscore rule for
 * names POSIX defines but C does not reserve. */
#define getpid _getpid

#endif
