// A file called string.h, sitting beside a source file, on purpose.
//
// It is the one thing no case in tests/cases can arrange: a single-file case
// lives in a directory shared with three hundred others, and a header named
// string.h there would be found by every one of them.
#ifndef LOCAL_STRING_H
#define LOCAL_STRING_H

int local_answer(void);

#endif
