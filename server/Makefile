target = server_TCP
skin = native

CFLAGS := $(shell xeno-config --skin=$(skin) --cflags)
LDFLAGS := $(shell xeno-config --skin=$(skin) --ldflags)

$(target): $(target).c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lcurses
clean:
	@rm $(target)

