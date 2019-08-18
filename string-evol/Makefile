CPP=g++
CPP98=-std=c++98 -pedantic
CPP17=-std=c++17

.PHONY: clean builddir all

all: vstring-cpp17 vstring-cpp98

vstring-cpp98: vstring-cpp98.cpp builddir
	$(CPP) -o build/vstring-cpp98 $(CPP98) vstring-cpp98.cpp

vstring-cpp17: vstring-cpp17.cpp builddir
	$(CPP) -o build/vstring-cpp17 $(CPP17) vstring-cpp17.cpp

builddir:
	mkdir -p build

clean:
	rm -fr build
