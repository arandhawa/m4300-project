CXX=g++
LINK=-lcurl
override CFLAGS += -g -ggdb -Wall -std=c++14

.PHONY: all
all: main getstock

main: main.cc
	$(CXX) $^ -o $@ $(CFLAGS) $(LINK)
getstock: getstock.cc
	$(CXX) $^ -o $@ $(CFLAGS) $(LINK)
clean:
	@echo cleaning
	@rm -f main getstock *.o
