#include "server.hpp"

int main()
{
    size_t port = 50000;
    vod::_log.info("main","server run at %d",port);
    vod::Server s(port);
    s.Run();
    return 0;
}