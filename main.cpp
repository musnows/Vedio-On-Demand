#include "server/server.hpp"
#define PORT 50000//服务端所用端口

int main()
{
    // 打印日志
    vod::_log.info("main","server run at %d",PORT);
    vod::Server s(PORT);
    s.Run();// 启动服务器
    return 0;
}