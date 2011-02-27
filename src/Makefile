CFLAGS = -Wall -Wextra -ansi

TMIPS_OBJS = config.o core.o error.o main.o mem.o ram.o readmemh.o serial.o util.o

tmips: $(TMIPS_OBJS)
	$(CC) $^ -o $@

clean:
	rm -f tmips $(TMIPS_OBJS)
