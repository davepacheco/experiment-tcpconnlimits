#
# Simple Makefile for client and server programs.
#

LDFLAGS += -lnsl -lpthread -lsocket
CCFLAGS += -Wall -Werror -Wextra

# Target for building an executable from a single C file.
MAKEEXE = $(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $^

.PHONY: all
all: client server

.PHONY: clean
clean:
	rm -f client server

client: client.c
	$(MAKEEXE)

server: server.c
	$(MAKEEXE)
