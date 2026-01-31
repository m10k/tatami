OUTPUT = wm
OBJECTS = array.o client.o common.o dispatcher.o event.o log.o main.o monitor.o \
          set.o wm.o workspace.o xrandr.o

ifeq ($(PREFIX), )
	PREFIX = /usr
endif
ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PKG_CONFIG_LIBS = xft pango pangoxft
CFLAGS = -pedantic -Wall #$(shell pkg-config --cflags $(PKG_CONFIG_LIBS))
LDFLAGS = -lX11 -lXrandr #$(shell pkg-config --libs $(PKG_CONFIG_LIBS))

PHONY = all clean install uninstall

ifeq ($(DEBUG), 1)
	CFLAGS += -g
	LDFLAGS += -g
else
	CFLAGS += -O2
endif

all: wm

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -oroot -groot -m755 wm $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/wm

wm: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS)

.PHONY: $(PHONY)
