TARG = check-journal

OFILES=\
	main.o\

CFLAGS=-Wall
LDFLAGS=-lsystemd

.PHONY: all
all: $(TARG)

check-journal: $(OFILES)
	$(LINK.o) -o $@ $<

.PHONY: clean
clean:
	rm -f $(OFILES)

.PHONY: nuke
nuke:
	rm -f $(OFILES) $(TARG)
