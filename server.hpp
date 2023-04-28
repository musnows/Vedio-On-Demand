#ifndef __VOD_SERVER__
#define __VOD_SERVER__
#include "httplib.h"
#include "data.hpp"
namespace vod
{
#define DEFAULT_SERVER_PORT 50000
    // 为了方便访问这个对象，定义全局变量
    VideoTb *video_table = NULL;
    // 服务器端
    class Server
    {
    private:
        Json::Value _conf;    // 服务器的配置文件（json）
        size_t _port;         // 服务器监听端口
        httplib::Server _srv; // 用于搭建http服务器
    private:
        // 将前端发过来的请求指定对应的业务处理接口，转给后端的mysql进行处理
        // 说白了就是把http协议中请求的参数给提取出来，然后调用对应data.hpp中的函数
        // 并给客户端提供正确响应
        // 这里写static是方便将函数映射给httplib
        static void Insert(const httplib::Request &req, httplib::Response &rsp);
        static void Update(const httplib::Request &req, httplib::Response &rsp);
        static void Delete(const httplib::Request &req, httplib::Response &rsp);
        // 通过视频id获取视频
        static void GetOne(const httplib::Request &req, httplib::Response &rsp);
        // 通过视频路径获取所有视频，或者用query参数来进行搜搜
        static void GetAll(const httplib::Request &req, httplib::Response &rsp);

    public:
        Server(size_t port=DEFAULT_SERVER_PORT)
            :_port(port)
        {
            std::string tmp_str;
            Json::Value conf;
            if(!FileUtil(CONF_FILEPATH).GetContent(&tmp_str)){
                //保证读取配置文件不要出错
                _log.fatal("server init","get config err");
                abort();
            }
            JsonUtil::UnSerialize(tmp_str, &conf);
            _conf = conf["web"];//赋值web的json格式给服务器，作为服务器配置
            _log.info("server init","init finished");
        }
        // 建立请求与处理函数的映射关系，设置静态资源根目录，启动服务器，
        bool Run()
        {
            //1.初始化数据管理模块，创建本地的资源文件夹
            video_table = new VideoTb();//新建一个表对象
            FileUtil(_conf["root"].asCString()).CreateDirectory();//创建根目录文件夹
            std::string root = _conf["root"].asString();
            std::string video_real_path = root + _conf["vedio_root"].asCString();// ./www/video/
            FileUtil(video_real_path).CreateDirectory();//创建文件夹
            std::string image_real_path = root + _conf["image_root"].asCString();// ./www/image/
            FileUtil(image_real_path).CreateDirectory();
            //2.调用httplib的接口，映射服务器表
            //2.1 设置静态资源根目录
            _srv.set_mount_point("/", _conf["root"].asCString());
            //2.2 添加请求-处理函数映射关系
            _srv.Post("/video", Insert);
            //   正则匹配
            _srv.Delete("/video/*", Delete);
            _srv.Put("/video/*", Update);
            _srv.Get("/video/*", GetOne);
            _srv.Get("/video", GetAll);
            //3.指定端口，启动服务器
            //  这里必须用0.0.0.0，否则其他设备无法访问
            _srv.listen("0.0.0.0", _port);
            return true;
        }

    };
}

#endif