.PHONY: all clean

all:
	$(CXX) -std=c++20 tcp_state.cpp -o tcp_state
	./tcp_state
	rm tcp_state

clean:
	rm -rf rfc9293 synack ack data data_random finack
