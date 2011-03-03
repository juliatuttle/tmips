CFLAGS = -Wall -Wextra -ansi -DDEBUG_TRACE_STEP

TMIPS_OBJS = config.o core.o core_cp0.o exc.o main.o mem.o ram.o readmemh.o serial.o util.o

tmips: $(TMIPS_OBJS)
	$(CC) $^ -o $@

clean:
	rm -f tmips $(TMIPS_OBJS)
