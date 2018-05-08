CXX=g++
override CFLAGS += -g -ggdb -std=c++14
EIGEN_ROOT=/usr/include/eigen3
.PHONY: all
all: main getstock cov

main: main.cc
	$(CXX) $^ -o $@ $(CFLAGS) -I$(EIGEN_ROOT)
getstock: getstock.cc
	$(CXX) $^ -o $@ $(CFLAGS) -lcurl
cov: cov.cc
	$(CXX) $^ -o $@ $(CFLAGS) -lcurl -I$(EIGEN_ROOT)
clean:
	@echo cleaning
	@rm -f main getstock cov *.o
