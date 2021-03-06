# Compiler flags
CC = gcc
CFLAGS = -g -O3 -std=c99 $(WFLAGS)
WFLAGS = -Wall -Wextra -pedantic -Wundef -Wunreachable-code -Wcast-align \
	 -Wstrict-prototypes -Wswitch-default -Wpointer-arith \
	 -Wswitch-enum -Wconversion
DFLAGS = -DDEBUG_MODE=0
INCLUDE = -isystem include

# Directories
PROG = forthjr
SRCDIR = src
BUILDDIR = build
TESTDIR = tests

SOURCES = $(shell find $(SRCDIR) -type f -name *.c)
OBJECTS_ALL = $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.c=.o))

# Recipes
all: $(PROG)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DFLAGS) $(INCLUDE) -c -o $@ $<

$(PROG): $(OBJECTS_ALL)
	$(CC) $(OBJECTS_ALL) -o $@

test: $(PROG)
	@$(TESTDIR)/harness ./$^

clean:
	@$(RM) *.o
	@$(RM) $(PROG)
	@$(RM) -r $(BUILDDIR)

.PHONY: clean
