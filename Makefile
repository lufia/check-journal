TARG = check-journal

OFILES=\
	exit.o\
	list.o\
	main.o\
	name.o\
	regexp.o\
	state.o\

HFILES=\
	lib.h\

CFLAGS=-Wall
LDFLAGS=-lsystemd

.PHONY: all
all: $(TARG)

check-journal: $(OFILES)
	$(LINK.o) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OFILES)

.PHONY: nuke
nuke:
	$(RM) $(OFILES) $(TARG)
