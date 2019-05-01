futex-test: futex-test.o
	g++ -pthread -o futex-test futex-test.o

futex-test.o: futex-test.cpp
	g++ -g -std=c++17 -c -o futex-test.o futex-test.cpp

clean:
	rm -rf futex-test futex-test.o

all: futex-test

.PHONY: clean all


