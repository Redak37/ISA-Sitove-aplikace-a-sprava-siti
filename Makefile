#/*
# *  File:	Makefile
# *  Solution:  ISA - HTTP nastenka
# *  Author:    Radek Duchoň - xducho07, VUT FIT 3BIT 2019/20
# *  Compiled:  g++ 7.4.0
# *  Datum:     12.11.2019
# */

CXX = g++
CXXFLAGS = -std=c++11 -pedantic -Wall -Wextra
TARGETS = isaclient isaserver 

all: $(TARGETS)

isaclient: isaclient.o
	$(CXX) -o $@ $^

isaserver: isaserver.o
	$(CXX) -o $@ $^

clean:
	@rm -f *.o $(TARGETS)

zip:
	#rm xducho07.zip
	zip xducho07.zip *.cpp *.hpp Makefile README manual.pdf
tar:
	rm xducho07.tar
	tar -cvf xducho07.tar *.cpp *.hpp Makefile README manual.pdf
