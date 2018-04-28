CXX=g++
LINK=-lcurl
override CFLAGS += -g -ggdb -Wall -std=c++14
getstock: getstock.cc
	$(CXX) $^ -o $@ $(CFLAGS) $(LINK)
clean:
	@echo cleaning
	@rm -f getstock *.o
