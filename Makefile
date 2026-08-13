# Built by g++.
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

CXX      = g++
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

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

src/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	@./tests/run.sh

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf tests/out
