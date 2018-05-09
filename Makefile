CXX=g++
CFLAGS = -g -ggdb -std=c++14
debug ?= yes
CFLAGS=-std=c++14
EIGEN_ROOT=/usr/include/eigen3

ifeq ($(debug),no)
	CFLAGS += -O2 -march=native -mtune=native -fomit-frame-pointer
else
	CFLAGS += -DDEBUG
endif

.PHONY: all
all: main getstock cov

main: main.cc
	$(CXX) $^ -o $@ $(CFLAGS) -fopenmp -I$(EIGEN_ROOT)
getstock: getstock.cc
	$(CXX) $^ -o $@ $(CFLAGS) -lcurl
cov: cov.cc
	$(CXX) $^ -o $@ $(CFLAGS) -lcurl -I$(EIGEN_ROOT)
clean:
	@echo cleaning
	@rm -f main getstock cov *.o
