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
CXXFLAGS = -std=c++17 -O2 -g -Wall -Wextra -Werror -pedantic -pthread \
           -DCC1_INCLUDE_DIR='"$(INCDIR)"'
SRCS     = $(wildcard src/*.cpp)
OBJS     = $(SRCS:.cpp=.o)
HDRS     = $(wildcard src/*.h)
TARGET   = cc1

.PHONY: all test clean help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The differential suite compiles every case a second time with gcc and runs
# both binaries, so it needs gcc and it needs to be able to run x86-64. On a Mac
# it says so rather than failing halfway through with something puzzling.
test: $(TARGET)
ifeq ($(UNAME_S),Darwin)
	@echo "The suite compares against gcc and runs x86-64 binaries, and this is"
	@echo "$(shell uname -m)-darwin. cc1 itself builds and runs here - it will"
	@echo "read C and write x86-64 assembly - but nothing on this machine can"
	@echo "assemble or run what it writes. Run 'make test' on the Linux box."
	@false
else
	@./tests/run.sh
endif

help:
	@echo "make            build cc1 with $(CXX)"
	@echo "make test       build and run the differential suite (Linux only)"
	@echo "make clean"
	@echo ""
	@echo "cc1 emits x86-64 System V assembly. It compiles anywhere this"
	@echo "Makefile does; its output runs where that ABI does."

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf tests/out
