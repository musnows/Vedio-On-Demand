main:main.cpp ./server/
	g++ main.cpp -o main -lmysqlclient -ljsoncpp -lpthread

.PHONY:clean
clean:
	rm main