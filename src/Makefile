


SOURCES_CPP=$(wildcard *.cpp)
SOURCES_C=$(wildcard *.c)

O_FILES=$(patsubst %.cpp,%.o, $(SOURCES_CPP))




all : exec
	

exec : $(O_FILES)
	mpic++  $(MAKEOPTS) -g -fopenmp -o $@ $^

%.o : %.cpp
	mpic++ -c $(MAKEOPTS) -o $@ $^ -std=c++0x -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused 

%.o : %.c 
	gcc -c -o $(MAKEOPTS) $@ $^ -std=gnu99 -Wall -Wextra -Werror -fopenmp -g -I. -Wno-unused


clean :
	rm -rf exec *.o
