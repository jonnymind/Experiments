futex-test: futex-test.o
	g++ -pthread -o futex-test futex-test.o

futex-test.s:
	g++ -fverbose-asm -S -O2 -std=c++17 -c -o futex-test.s futex-test.cpp

futex-test.o: futex-test.cpp
	g++ -O2 -g -std=c++17 -c -o futex-test.o futex-test.cpp

clean:
	rm -rf futex-test futex-test.o futex-test.s

all: futex-test

.PHONY: clean all


