
CXX = g++ -fPIC

all: HashTableVoidTest IRCServer

HashTableVoidTest: HashTableVoidTest.cc HashTableVoid.cc
	g++ -g -o HashTableVoidTest HashTableVoidTest.cc HashTableVoid.cc

IRCServer: IRCServer.cc

clean:
	rm -f *.out
	rm -f *.o HashTableVoidTest IRCServer


