CC := clang++-15

.PHONY: all asm
all: no_unlikely unlikely
asm: no_unlikely.s unlikely.s

%: %.cpp
	$(CC) -O2 $^ -o $@

%.s: %.cpp
	$(CC) -O2 -S $^ -o $@

.PHONY: clean
clean:
	@rm -f no_unlikely
	@rm -f unlikely
	@rm -f perf*
