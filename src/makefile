


SOURCES_CPP=$(wildcard *.cpp)
SOURCES_C=$(wildcard *.c)

O_FILES=$(patsubst %.c,%.o, $(SOURCES_C)) $(patsubst %.cpp,%.o, $(COUSRCES_CPP))


exec : $(O_FILES)
	g++ -o $@ $^

%.o : %.cpp
	g++ -c -o $@ $^ -std=c++0x -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused

%.o : %.c 
	gcc -c -o $@ $^ -std=gnu99 -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused

