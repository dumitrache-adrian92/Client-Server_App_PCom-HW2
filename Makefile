all: server subscriber

server: server.cpp simple_io.cpp
	g++ -std=c++17 -Wall -Wextra -o server server.cpp simple_io.cpp -O2

subscriber: subscriber.cpp simple_io.cpp
	g++ -std=c++17 -Wall -Wextra -o subscriber subscriber.cpp simple_io.cpp -O2

clean:
	rm -f server subscriber server_log client_log

run_server: server.cpp simple_io.cpp
	./server 12345 2> server_log.txt