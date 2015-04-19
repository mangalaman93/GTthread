#### GTThread Library Makefile

CFLAGS  = -Wall -pedantic -g
LFLAGS  =
CC      = gcc
RM      = /bin/rm -rf
AR      = ar rc
RANLIB  = ranlib

TEST = phil_test rand_test

LIBRARY = gtthread.a

LIB_SRC = gtthread.c

LIB_OBJ = $(patsubst %.c,%.o,$(LIB_SRC))

# pattern rule for object files
%.o: %.c
	$(CC) -c $(CFLAGS) $(LFLAGS) $< -o $@

all: $(LIBRARY) $(TEST)

$(LIBRARY): $(LIB_OBJ)
	$(AR) $(LIBRARY) $(LIB_OBJ)
	$(RANLIB) $(LIBRARY)

%_test: $(LIBRARY) %_test.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $@.c $(LIBRARY)

clean:
	$(RM) $(LIBRARY) $(LIB_OBJ) $(TEST)

.PHONY: depend
depend:
	$(CFLAGS) $(LFLAGS) -- $(LIB_SRC) 2>/dev/null
