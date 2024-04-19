TARG=check-journal

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
LDFLAGS=
LDLIBS=-lsystemd

.PHONY: all
all: $(TARG)

$(TARG): $(OFILES)
	$(LINK.o) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	$(RM) $(OFILES)

.PHONY: nuke
nuke:
	$(RM) $(OFILES) $(TARG)
