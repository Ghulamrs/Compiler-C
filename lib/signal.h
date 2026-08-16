// signal.h - handling an asynchronous signal.
//
// This header is the reason 'a function returning a function pointer' had to
// be implemented before it could be written. signal takes a handler and gives
// back the previous one, so its declaration is
//
//     void (*signal(int sig, void (*handler)(int)))(int);
//
// which reads outwards from the name: signal is a function, taking an int and
// a pointer to a function taking int and returning void, and returning a
// pointer to a function taking int and returning void. It is the standard
// library's most-quoted declaration and it is not a puzzle, it is just C's
// declarator grammar meaning what it says.
//
// SIGABRT is the one number here that moves: 6 on macOS and Linux, 22 under
// the UCRT. The other five agree on all three, which was measured rather than
// assumed.
#ifndef _CC1_SIGNAL_H
#define _CC1_SIGNAL_H

// An integer type that can be read and written atomically with respect to a
// signal arriving. Four bytes on all three targets. A variable a handler
// touches should be 'volatile sig_atomic_t' and nothing else.
typedef int sig_atomic_t;

#define SIGINT      2
#define SIGILL      4
#define SIGFPE      8
#define SIGSEGV     11
#define SIGTERM     15

#ifdef _WIN32
#define SIGABRT     22
#else
#define SIGABRT     6
#endif

// The three special handler values, which are addresses rather than numbers
// and so are cast. SIG_ERR is what signal returns when it fails.
#define SIG_DFL     ((void (*)(int))0)
#define SIG_IGN     ((void (*)(int))1)
#define SIG_ERR     ((void (*)(int))-1)

void (*signal(int sig, void (*handler)(int)))(int);
int raise(int sig);

#endif
