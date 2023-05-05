#include "server/server.hpp"
#define PORT 50008 //服务端所用端口

int main()
{
    // 打印日志
    printf("\n");
    vod::_log.info("main","server run at http://0.0.0.0:%d",PORT);
    // 服务器对象
    vod::Server s(PORT);
    if(!s.Run()){// 启动服务器
        vod::_log.fatal("main","server run failed! main exit");
        return -1;
    }

    return 0;
}