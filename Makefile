PREFIX=/usr/local

all: broadcat

broadcat: broadcat.c

install: all
	install -m 0755 broadcat $(PREFIX)/bin/

clean:
	rm broadcat
