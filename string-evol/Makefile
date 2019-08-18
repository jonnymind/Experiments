CPP=g++
CPP98=-std=c++98 -pedantic

.PHONY: clean builddir

vstring-cpp98: vstring-cpp98.cpp builddir
	$(CPP) -o build/vstring-cpp98 $(CPP98) vstring-cpp98.cpp

builddir:
	mkdir -p build

clean:
	rm -fr build

all: vstring-cpp98
