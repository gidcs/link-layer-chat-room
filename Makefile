CC 	= gcc
CXX = g++ -std=c++11
PROGS = main
CFLAGS = -Wall -g -pthread

all: $(PROGS)

%.o: $.c
	$(CC) -o $@ $(CFLAGS) $<

%.o: $.cpp
	$(CXX) -o $@ $(CFLAGS) $<

$(PROGS): main.o
	$(CXX) -o $@ $(CFLAGS) $< 

clean:
	rm -rf *.o $(PROGS)
