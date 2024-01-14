CC := clang++-15

all: no_unlikely unlikely

%: %.cpp
	$(CC) -O2 $^ -o $@

clean:
	rm no_unlikely
	rm unlikely
