TARG = check-journal

OFILES=\
	exit.o\
	main.o\
	name.o\
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
