main:main.cpp ./server/
	g++ main.cpp -o vod-server -lmysqlclient -ljsoncpp -lpthread

.PHONY:test
test:
	./vod-server

.PHONY:run
run:
	nohup ./vod-server >> server.log 2>&1 &

.PHONY:clean
clean:
	rm vod-server