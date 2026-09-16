CC = g++ -std=c++20
CFLAGS = -Wall -Wfatal-errors 
LDFLAGS = -I/opt/homebrew/include/SDL2 -I/opt/homebrew/include
LDLIBS = -Iinclude -lSDL2main -lSDL2 -L/opt/homebrew/lib -lSDL2_ttf -lSDL2_image -lsqlite3 -I/opt/homebrew/opt/openssl@3/include -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -lfmt

all: server.x client_account.x read_database.x

read_database.x: read_database.o
	$(CC) $(CFLAGS) -o $@ $< -lsqlite3

server.x: server.o action.o client.o database_management.o market.o messages.o order.o utility.o
	$(CC) $(CGFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

client_account.x: client_account.o database_management.o messages.o utility.o
	$(CC) $(CGFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CC) $(CGFLAGS) $(LDFLAGS) -o $@ -c $< $(LDLIBS)

clean:
	rm -f *.o

realclean: clean
	rm -f *.x