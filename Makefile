OUT := qmic

CFLAGS := -Wall -g
LDFLAGS :=

SRCS := qmic.c qmi_message.c qmi_struct.c
OBJS := $(SRCS:.c=.o)

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

test: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT) $(OBJS)

