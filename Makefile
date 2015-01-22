#### GTThread Library Makefile

CFLAGS  = -Wall -pedantic
LFLAGS  =
CC      = gcc
RM      = /bin/rm -rf
AR      = ar rc
RANLIB  = ranlib

TEST = phil

LIBRARY = gtthread.a

LIB_SRC = gtthread.c

LIB_OBJ = $(patsubst %.c,%.o,$(LIB_SRC))

# pattern rule for object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

all: $(LIBRARY) $(TEST)

$(LIBRARY): $(LIB_OBJ)
	$(AR) $(LIBRARY) $(LIB_OBJ)
	$(RANLIB) $(LIBRARY)

$(TEST): $(LIBRARY) $(TEST).c
	$(CC) -o $(TEST) $(TEST).c $(LIBRARY)

clean:
	$(RM) $(LIBRARY) $(LIB_OBJ) $(TEST)

.PHONY: depend
depend:
	$(CFLAGS) -- $(LIB_SRC) 2>/dev/null