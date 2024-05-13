.PHONY: all clean

DIR = output
ALL_CPP = $(wildcard *.cpp)
ALL_BIN = $(patsubst %.cpp,%,$(ALL_CPP))

%: %.cpp
	@mkdir -p $(DIR)
	-$(CXX) -std=c++20 -Wall -Wextra $^ -o $(DIR)/$@

all: $(ALL_BIN)

clean:
	@rm -r $(DIR)
