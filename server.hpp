#ifndef __VOD_SERVER__
#define __VOD_SERVER__
#include "httplib.h"
#include "data.hpp"
namespace vod
{
#define DEFAULT_SERVER_PORT 50000
#define NAME_TIME_FORMAT "%y%m%d%H%M%S"      // 给文件命名时所用的时间格式
#define DEFAULT_VEDIO_INFO "这里什么都没有~" // 默认视频简介
#define DEFAULT_VEDIO_COVER "default.png"    // 默认视频封面
    // 为了方便访问这个对象，定义全局变量
    // 用大写方便标识
    VideoTb VideoTable;
    Json::Value Conf; // 服务器的配置文件（json）
    // 服务器端
    class Server
    {
    private:
        size_t _port;         // 服务器监听端口
        httplib::Server _srv; // 用于搭建http服务器
        // 出现数据库写入异常的通用处理
        static void MysqlErrHandler(const std::string &def_name, httplib::Response &rsp)
        {
            rsp.status = 500;
            rsp.body = R"({"code":500, "message":"视频信息写入数据库失败"})";
            rsp.set_header("Content-Type", "application/json");
            _log.error(def_name, "database query err!");
            return;
        }
        // 检查是否却少键值
        static bool ReqKeyCheck(const std::string &def_name, const std::vector<const char *> key_array, const httplib::Request &req, httplib::Response &rsp)
        {
            for (auto &key : key_array)
            {
                if (req.has_file(key) == false)
                {
                    rsp.status = 400; // 该状态码代表服务器不理解请求的含义
                    // R代表原始字符串，忽略字符串内部的特殊字符。如果不用R，就需要\"转义双引号，麻烦
                    rsp.body = R"({"code":500, "message":"上传的数据信息错误，键值缺失"})";
                    rsp.set_header("Content-Type", "application/json");
                    _log.warning(def_name, "missing key '%s'", key);
                    return false;
                }
            }
            return true;
        }

    private:
        // 将前端发过来的请求指定对应的业务处理接口，转给后端的mysql进行处理
        // 说白了就是把http协议中请求的参数给提取出来，然后调用对应data.hpp中的函数
        // 并给客户端提供正确响应
        // 这里写static是方便将函数映射给httplib
        static void Insert(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("server insert", "post recv from %s", req.remote_addr.c_str());
            static const std::vector<const char *> ins_key = {"name", "info", "video", "cover"};
            // 使用循环来判断文件中是否含有这些字段，没有就返回400
            if(!ReqKeyCheck("server insert",ins_key,req,rsp))return;
            // 从请求的req中取出对应的字段，
            httplib::MultipartFormData name = req.get_file_value("name");   // 视频名称
            httplib::MultipartFormData info = req.get_file_value("info");   // 视频简介
            httplib::MultipartFormData video = req.get_file_value("video"); // 视频文件
            httplib::MultipartFormData image = req.get_file_value("cover"); // 视频封面
            // 获取视频标题和简介的内容
            std::string video_name = name.content;
            std::string video_info = info.content;
            // 视频名不能为空，简介为空替换为默认简介
            if (video_name.size() == 0 || video.content.size() == 0)
            {
                rsp.status = 400; // 客户端出错
                rsp.body = R"({"code":400, "message":"视频名字/视频文件不能为空"})";
                rsp.set_header("Content-Type", "application/json");
                _log.warning("server insert", "empty video name or video content");
                return;
            }
            // 视频简介为空/视频封面为空替换成默认简介
            if (video_info.size() == 0)
            {
                video_info = DEFAULT_VEDIO_INFO;
                _log.warning("server insert", "empty video info, using default");
            }
            // 拼接视频路径
            std::string root = Conf["root"].asString();
            // 视频名中包含时间和视频文件的名字，对于本项目而言，安秒记录的时间已经能保证视频文件路径的唯一性了
            // 如果是请求量更大的项目，可以考虑使用毫秒级时间戳，或视频文件的md5字符串作为视频的路径名
            std::string video_path = Conf["video_root"].asString() + GetTimeStr(NAME_TIME_FORMAT) + video.filename;
            std::string image_path = Conf["image_root"].asString() + GetTimeStr(NAME_TIME_FORMAT) + image.filename;
            // 将文件写入path
            if (!FileUtil(root + video_path).SetContent(video.content))
            {
                rsp.status = 500; // 服务端出错
                rsp.body = R"({"code":500, "message":"视频文件存储失败"})";
                rsp.set_header("Content-Type", "application/json");
                _log.error("server insert", "write video file err! path:%s", video_path.c_str());
                return;
            }
            if (!FileUtil(root + image_path).SetContent(image.content))
            {
                rsp.status = 500; // 服务端出错
                rsp.body = R"({"code":500, "message":"视频封面图片文件存储失败"})";
                rsp.set_header("Content-Type", "application/json");
                _log.error("server insert", "write cover image file err! path:%s", video_path.c_str());
                return;
            }
            _log.info("server insert", "write file video:'%s' cover:'%s'", video_path.c_str(), image_path.c_str());
            Json::Value video_json;
            video_json["name"] = video_name;
            video_json["info"] = video_info;
            video_json["video"] = video_path;
            video_json["cover"] = image_path;
            // 注意json的键值不能出错，否则会抛出异常（异常处理太麻烦了）
            if (!VideoTable.Insert(video_json))
                return MysqlErrHandler("server insert", rsp);
            // 上传成功
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"上传视频成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("server insert", "database insert finished! path:%s", video_path.c_str());
            // rsp.set_redirect("/index.html", 303); // 将用户重定向到主页
            return;
        }
        // 更新视频，暂时只支持更新视频标题和简介
        static void Update(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("server update", "put recv from %s", req.remote_addr.c_str());
            static const std::vector<const char *> upd_key = {"name", "info"};
            // 使用循环来判断文件中是否含有这些字段，没有就返回400
            if(!ReqKeyCheck("server update",upd_key,req,rsp))return;
            std::string video_id = req.matches[1];//从匹配的正则中获取到视频id
            _log.info("server update","video id recv id:%s",video_id.c_str());
            // 取出对应内容
            httplib::MultipartFormData name = req.get_file_value("name");   // 视频名称
            httplib::MultipartFormData info = req.get_file_value("info");   // 视频简介
            // 获取视频标题和简介的内容
            std::string video_name = name.content;
            std::string video_info = info.content;
            Json::Value video;
            video["name"] = video_name;
            video["info"] = video_info;
            if(!VideoTable.Update(video_id,video))
                return MysqlErrHandler("server update", rsp);
            // 更新成功
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"更新视频标题/简介成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("server insert", "database insert finished! id:%s", video_id.c_str());
            return;
        }
        // static void Delete(const httplib::Request &req, httplib::Response &rsp);
        // // 通过视频id获取视频
        // static void GetOne(const httplib::Request &req, httplib::Response &rsp);
        // // 通过视频路径获取所有视频，或者用query参数来进行搜搜
        // static void GetAll(const httplib::Request &req, httplib::Response &rsp);

    public:
        Server(size_t port = DEFAULT_SERVER_PORT)
            : _port(port)
        {
            std::string tmp_str;
            Json::Value conf;
            if (!FileUtil(CONF_FILEPATH).GetContent(&tmp_str))
            {
                // 保证读取配置文件不要出错
                _log.fatal("server init", "get config err");
                abort();
            }
            JsonUtil::UnSerialize(tmp_str, &conf);
            Conf = conf["web"]; // 赋值web的json格式给服务器，作为服务器配置
            _log.info("server init", "init finished");
        }
        // 建立请求与处理函数的映射关系，设置静态资源根目录，启动服务器，
        bool Run()
        {
            // 1.创建本地的资源文件夹
            FileUtil(Conf["root"].asString()).CreateDirectory(); // 创建根目录文件夹
            std::string root = Conf["root"].asString();
            std::string video_real_path = root + Conf["video_root"].asString(); // ./www/video/
            FileUtil(video_real_path).CreateDirectory();                         // 创建文件夹
            std::string image_real_path = root + Conf["image_root"].asString(); // ./www/image/
            FileUtil(image_real_path).CreateDirectory();
            // 2.调用httplib的接口，映射服务器表
            // 2.1 设置静态资源根目录
            _srv.set_mount_point("/", Conf["root"].asString());
            // 2.2 添加请求-处理函数映射关系
            _srv.Post("/video", Insert);
            // //   正则匹配
            // _srv.Delete("/video/*", Delete);
            _srv.Put("/video/([A-Za-z0-9]+)", Update);
            // _srv.Get("/video/*", GetOne);
            // _srv.Get("/video", GetAll);
            // 3.指定端口，启动服务器
            //  这里必须用0.0.0.0，否则其他设备无法访问
            _srv.listen("0.0.0.0", _port);
            return true;
        }
    };
}

#endif