main:main.cpp ./server/
	g++ main.cpp -o main-server -lmysqlclient -ljsoncpp -lpthread

.PHONY:run
run:
	./main-server

.PHONY:clean
clean:
	rm main-server