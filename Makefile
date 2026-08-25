# Built by g++ on the development box, and by clang++ on a Mac.
#
# Both work and both are checked: every translation unit compiles under
# -Wall -Wextra -Werror -pedantic with Apple clang as well as with GNU g++, and
# the compiler that comes out is the same program. What differs is what you can
# then do with it - see "make help".
#
# Serial by design, and not merely as a preference. This box has 419 MiB of RAM
# and a class-heavy C++ translation unit was measured at 178-195 MB to compile;
# -j2 asks for twice that and meets the OOM killer. Use ./build rather than
# calling make directly - it puts the whole build inside a memory cgroup, so a
# runaway compile dies by itself instead of taking the machine down. That is
# not hypothetical: an unbounded `dnf` did exactly that on 12 August.
#
# Two different -j live in this repository and they are not related. This one is
# make's, building the compiler, and it stays at 1 because a C++ translation
# unit here costs 142 MB. cc1's own -j compiles several C files at once and is
# nothing like as hungry: a whole unit peaks at 4 MB.

# The host decides the compiler unless you say otherwise: "make CXX=g++-14" and
# "make CXX=clang++" both work anywhere either exists.
# origin, not ?=: make defines CXX itself, so ?= never fires. This overrides
# make's own default while still letting "make CXX=..." win.
UNAME_S := $(shell uname -s)
ifeq ($(origin CXX),default)
  ifeq ($(UNAME_S),Darwin)
    CXX := clang++
  else
    CXX := g++
  endif
endif

# The headers cc1 ships live in lib/, and are found by an absolute path baked in
# here because nothing installs this compiler - it runs from the tree it was
# built in. Taken from $(CURDIR) rather than written down, so a clone built
# somewhere else finds its own lib/ and not the one belonging to the tree this
# was written in.
#
# lib/ rather than include/, because none of what is in there is the language.
# The compiler is src/; the library it happens to ship is a separate thing that
# a program may ignore, replace with -I, or never reach for at all.
INCDIR   = $(CURDIR)/lib
# -pthread and not -lpthread: it sets the flags std::thread needs at compile
# time as well as naming the library, and getting only the library gives a
# binary that links and then misbehaves when it runs its threads.
#
# c++14, and src/ is written to it. The one thing that ever wanted C++17 was
# std::string_view, for the borrowed text an operand carries; src/backend/
# Spelling.h has a small Str of its own in its place, and nothing else in src/
# reaches past C++14. Apple's libc++ hands you string_view in C++14 mode
# anyway, so a Mac build will not catch that kind of slip - g++ on the box
# will, which is the reason to build there before believing it.
CXXFLAGS = -std=c++14 -O2 -g -Wall -Wextra -Werror -pedantic -pthread \
           -DCC1_INCLUDE_DIR='"$(INCDIR)"'
# src/backend holds one file per platform: the sizes its types measure, the ABI
# facts the front end has to know, and the code generator when there is one.
SRCS     = $(wildcard src/*.cpp) $(wildcard src/backend/*.cpp)
# Objects and their dependency files go under obj/ rather than beside the
# sources they came from, so that a listing of src/ is the code and nothing
# else. The tree under obj/ mirrors src/ - src/backend/X86_64.cpp becomes
# obj/backend/X86_64.o - so two files of the same name in different directories
# cannot collide, which a flat object directory would let them do.
#
# obj/ and not build/: there is already a script called build at the root.
OBJDIR   = obj
OBJS     = $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS     = $(OBJS:.o=.d)
# Where the finished program goes. `.` is this directory, which is what every
# suite, script and habit here already expects - so a plain `make` is unchanged
# by this being a parameter at all. What it buys is that the workspace build
# can name one directory and have all three programs built into it, rather than
# building them in three places and collecting them afterwards. A collection
# step is a step that can be forgotten, and was.
BINDIR  ?= .

# cc1.exe on every machine, not only Windows. The three programs in this family
# - RStudio, cc1 and shc - carry one name each wherever they are, and a
# suffix that changes by platform is one more thing a script has to know.
TARGET   = $(BINDIR)/cc1.exe

.PHONY: all test clean help

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

# -MMD -MP writes obj/X.d beside obj/X.o saying which headers went into it, and
# -include below reads them back. That replaces a rule that made every object
# depend on every header: correct, but it rebuilt all fifteen whenever any
# header was touched, and - worse - it was a list this file kept by hand.
#
# Getting this wrong is not a link error. A stale object compiled against an
# older class layout links perfectly well, because the mangled names still
# match, and the program then misbehaves somewhere else entirely. Compiler-S
# had exactly that: half its translation units keeping an old layout, and the
# compiler corrupting its own heap three passes away.
$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# The differential suite compiles every case a second time with gcc and runs
# both binaries, so it needs gcc and it needs to be able to run x86-64. On a Mac
# it says so rather than failing halfway through with something puzzling.
#
# Two suites run here, because two of the three backends emit x86-64. The
# second is x86_64-windows, which this machine can also execute: a
# Windows-convention program that calls no library is a self-contained blob,
# and tests/windows.sh explains at length why that is sound. The third backend
# is arm64-darwin and its suite runs on the Mac - "make test" there says so.
test: $(TARGET)
ifeq ($(UNAME_S),Darwin)
	@echo "This suite compares against gcc and runs x86-64 binaries, and this is"
	@echo "$(shell uname -m)-darwin. Run 'make test' on the Linux box for it."
	@echo ""
	@echo "What does run here: './tests/arm64.sh' builds and executes the native"
	@echo "backend's cases against clang, './tests/debug.sh' asks lldb where it"
	@echo "stopped, './tests/windows-native.sh' relays"
	@echo "the Windows corpus to a Windows machine over ssh, and"
	@echo "'./tests/fingerprint.sh' checks every byte of every target's assembly"
	@echo "against what is recorded - it needs no assembler, so it runs anywhere."
	@echo "A bare 'cc1 f.c' targets this Mac now, so"
	@echo "'cc1 f.c -o f.s && clang f.s -o f' works."
	@false
else
	@./tests/run.sh
	@./tests/windows.sh
	@./tests/driver-modes.sh
	@./tests/debug.sh
# The same debug corpus against the Microsoft ABI, which this machine can both
# build and run - see tests/windows.sh for why a Windows-convention program
# that calls no library executes here. It is the GNU spelling that carries the
# line table; ml64 wants CodeView, which is not written yet.
	@./tests/debug.sh x86_64-windows
	@./tests/fingerprint.sh
endif

help:
	@echo "make            build cc1 with $(CXX)"
	@echo "make test       build and run the differential suite (Linux only)"
	@echo "make clean"
	@echo ""
	@echo "cc1 emits x86-64 System V assembly. It compiles anywhere this"
	@echo "Makefile does; its output runs where that ABI does."

clean:
	rm -rf $(OBJDIR) $(TARGET)
	rm -rf tests/out tests/out-windows tests/out-arm64 \
	       tests/out-c90 tests/out-not-c90 tests/out-fingerprint \
	       tests/out-driver tests/out-debug tests/out-debug-x86_64-windows \
	       tests/out-cross tests/out-masm-native tests/out-windows-native
# A suite added since this rule was written leaves its output behind, and the
# list is the only place that says so. tests/out-cross survived a clean until
# 2026-08-26 for exactly that reason - eight object files nobody was looking
# for. Any new tests/out-* belongs on the line above the moment it exists.
