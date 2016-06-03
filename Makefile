OUT := qmic

CFLAGS := -Wall -g -O2
LDFLAGS :=

SRCS := qmic.c qmi_message.c qmi_struct.c
OBJS := $(SRCS:.c=.o)

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

install: $(OUT)
	install -D -m 755 $< $(PREFIX)/bin/$<

clean:
	rm -f $(OUT) $(OBJS)

