OUTPUT = tatami
OBJECTS = array.o client.o common.o dispatcher.o event.o layout.o log.o main.o monitor.o \
          set.o wm.o workspace.o xrandr.o

ifeq ($(PREFIX), )
	PREFIX = /usr
endif
ifeq ($(MANPREFIX), )
	MANPREFIX = $(PREFIX)/share/man
endif

PKG_CONFIG_LIBS = xft pango pangoxft
CFLAGS = -pedantic -Wall
LDFLAGS = -lX11 -lXrandr

PHONY = all clean install uninstall

ifeq ($(DEBUG), 1)
	CFLAGS += -g
	LDFLAGS += -g
else
	CFLAGS += -O2
endif

all: $(OUTPUT)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -oroot -groot -m755 tatami $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/tatami

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS)

.PHONY: $(PHONY)
