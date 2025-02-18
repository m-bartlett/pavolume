CC      =  g++
CFLAGS  += -std=c++17 -I$(PREFIX)/include
CFLAGS  += -D_POSIX_C_SOURCE=200112L
CFLAGS  += $(shell pkg-config --cflags libnotify)
LDFLAGS += -L$(PREFIX)/lib

LIBS     = -lm -lpulse
LIBS		 += $(shell pkg-config --libs libnotify)
TARGET   = pavolumenotify

PREFIX    ?= /usr/local
BINPREFIX  = $(PREFIX)/bin

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))

all: clean $(TARGET)

debug: CFLAGS += -O0 -g
debug: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.cpp $(LIBS)

install:
	install -m 755 -D --target-directory "$(BINPREFIX)" "$(TARGET)"

uninstall:
	rm -f "$(BINPREFIX)/$(TARGET)"

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all debug default install uninstall clean
# .PRECIOUS: $(TARGET) $(OBJECTS)
