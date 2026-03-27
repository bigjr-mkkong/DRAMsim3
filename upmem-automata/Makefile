CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I../src
TARGET   := upmem_automata
SRCS     := upmem_automata.cc main.cc
OBJS     := $(SRCS:.cc=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

upmem_automata.o: upmem_automata.cc upmem_automata.h
main.o: main.cc upmem_automata.h

clean:
	rm -f $(OBJS) $(TARGET)
