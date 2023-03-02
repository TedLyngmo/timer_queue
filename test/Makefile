CPPS = $(wildcard example*.cpp)
OBJS = $(CPPS:.cpp=.o)
EXES = $(CPPS:.cpp=)

CXXVER := -std=c++17

GCCOPTS := -Wall -Wextra -Werror
CLANGOPTS := -Weverything -Wno-c++98-compat -Wno-padded -Wno-weak-vtables -Wno-ctad-maybe-unsupported
OPTS = -o $@ $< $(CXXVER) -O3 -I../include -pedantic-errors -g -fsanitize=thread -pthread

test : ut-g++ ut-clang++
	./ut-g++ && ./ut-clang++

ut-g++ : ut.cpp Makefile ../include/lyn/timer_queue.hpp
	g++ $(OPTS) $(GCCOPTS)

ut-clang++ : ut.cpp Makefile ../include/lyn/timer_queue.hpp
	clang++ $(OPTS) $(CLANGOPTS)

format:
	clang-format -i *.hpp *.cpp

clean:
	rm -f $(EXES) $(OBJS)
