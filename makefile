main:main.cpp ./server/
	g++ main.cpp -o vod-server -lmysqlclient -ljsoncpp -lpthread

.PHONY:test
test:
	./vod-server

.PHONY:run
run:
	nohup ./vod-server >> server.log 2>&1 &

.PHONY:ps
ps:
	ps jax | head -1 && ps jax | grep vod-server | grep -v grep

.PHONY:clean
clean:
	rm vod-server