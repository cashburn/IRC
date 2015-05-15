
goal: IRCClient IRCServer

IRCClient: IRCClient.c
	gcc -std=gnu99 -pthread -g -o IRCClient IRCClient.c `pkg-config --cflags --libs gtk+-2.0`

IRCServer: IRCServer.cc IRCServer.h HashTableVoid.cc HashTableVoid.h
	g++ -g -o IRCServer IRCServer.cc

clean:
	rm -f hello panned entry radio timer TestIRCServer

