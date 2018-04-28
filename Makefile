CXX=g++
LINK=-lcurl
override CFLAGS += -g -ggdb -Wall -std=c++14
main: main.cc
	$(CXX) $^ -o $@ $(CFLAGS) $(LINK)
clean:
	@echo cleaning
	@rm -f main *.o
