# Built by g++.
#
# Serial by design, and not merely as a preference. This box has 419 MiB of RAM
# and a class-heavy C++ translation unit was measured at 178-195 MB to compile;
# -j2 asks for twice that and meets the OOM killer. Use ./build rather than
# calling make directly - it puts the whole build inside a memory cgroup, so a
# runaway compile dies by itself instead of taking the machine down. That is
# not hypothetical: an unbounded `dnf` did exactly that on 12 August.

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -g -Wall -Wextra -Werror -pedantic
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
