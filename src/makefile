


SOURCES_CPP=$(wildcard *.cpp)
SOURCES_C=$(wildcard *.c)

O_FILES=$(patsubst %.cpp,%.o, $(SOURCES_CPP))


exec : $(O_FILES)
	mpic++ -o $@ $^

%.o : %.cpp
	mpic++ -c -o $@ $^ -std=c++0x -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused

%.o : %.c 
	gcc -c -o $@ $^ -std=gnu99 -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused

