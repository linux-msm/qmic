OUT := qmic

CFLAGS ?= -Wall -g -O2
LDFLAGS ?=
prefix ?= /usr/local

SRCS := accessor.c kernel.c parser.c qmic.c
OBJS := $(SRCS:.c=.o)

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

install: $(OUT)
	install -D -m 755 $< $(DESTDIR)$(prefix)/bin/$<

clean:
	rm -f $(OUT) $(OBJS)

