build:
	g++ -g server.cpp -o server
	g++ -g client.cpp -o subscriber

server:
	g++ -g server.cpp -o server

subscriber:
	g++ -g client.cpp -o subscriber

clean:
	rm server subscriber
