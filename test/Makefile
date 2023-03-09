SRC := src
DRIVER := $(SRC)/main.cpp
DRIVERSRC = $(wildcard $(SRC)/ut*.cpp)
CPPS = $(DRIVER) $(DRIVERSRC)

BUILDDIR := build
GCCOBJS = $(CPPS:$(SRC)/%.cpp=$(BUILDDIR)/%.g++.o)
CLANGOBJS = $(CPPS:$(SRC)/%.cpp=$(BUILDDIR)/%.clang++.o)
OBJS = $(GCCOBJS) $(CLANGOBJS)

EXES = test-g++ test-clang++

STD = c++17
CXXVER := -std=$(STD)

INCLUDES = -I../include -I.

COMMONDEPS := Makefile ../include/lyn/timer_queue.hpp genmain.pl | builddir

OPTS = -o $@ $(CXXVER) -O3 $(INCLUDES) -pedantic-errors -g -pthread -Werror
GCCOPTS = -fsanitize=address,undefined,leak -Wall -Wextra $(OPTS)
CLANGOPTS = -stdlib=libc++ -fsanitize=thread -Weverything -Wno-c++98-compat $(OPTS)

TCOPTS = -Wno-unused-member-function -Wno-unused-template
GCCTCOPTS = $(GCCOPTS) $(TCOPTS)
CLANGTCOPTS = $(CLANGOPTS) $(TCOPTS) -Wno-padded -Wno-weak-vtables -Wno-ctad-maybe-unsupported -Wno-missing-prototypes -Wno-unused-member-function

test : $(EXES)
	./test-g++ && ./test-clang++ && echo OK

test-g++ : $(GCCOBJS)
	g++ $(GCCOBJS) $(GCCOPTS)

test-clang++ : $(CLANGOBJS)
	clang++ $(CLANGOBJS) $(CLANGOPTS)

$(BUILDDIR)/%.g++.o : $(SRC)/%.cpp $(COMMONDEPS)
	g++ -c $(GCCTCOPTS) $<

$(BUILDDIR)/%.clang++.o : $(SRC)/%.cpp $(COMMONDEPS)
	clang++ -c $(CLANGTCOPTS) $<

$(DRIVER) : ./genmain.pl $(DRIVERSRC) builddir
	./genmain.pl $(CPPS) > $@

builddir:
	mkdir -p $(BUILDDIR)

tidy : $(CPPS)
	clang-tidy -p . $(CPPS) -extra-arg=-$(CXXVER) -- $(INCLUDES)

format:
	clang-format -i *.cpp

clean:
	rm -rf $(DRIVER) $(EXES) $(BUILDDIR)
