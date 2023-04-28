main:main.cpp utils.hpp mylog.hpp data.hpp server.hpp
	g++ main.cpp -o main -lmysqlclient -ljsoncpp -lpthread

.PHONY:clean
clean:
	rm main