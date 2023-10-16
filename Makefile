user_progs := fib fib2 fib3 fib4 fib5 fib6 fib7
CC = gcc

default: shell scheduler $(user_progs)

shell: shell.c common.h
	$(CC) $(CFLAGS) shell.c -o shell

scheduler: scheduler.c common.h
	$(CC) $(CFLAGS) scheduler.c -o scheduler

$(user_progs): %: %.c dummy_main.h
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f shell scheduler $(user_progs)