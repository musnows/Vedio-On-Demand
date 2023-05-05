#ifndef __VOD_SERVER__
#define __VOD_SERVER__
#include "httplib.h"
#include "data/mysql.hpp"
#include "data/sqlite3.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace vod
{
#define DEFAULT_SERVER_PORT 50000
#define NAME_TIME_FORMAT "%y%m%d%H%M%S"      // 给文件命名时所用的时间格式
#define DEFAULT_VEDIO_INFO "这里什么都没有~" // 默认视频简介
#define DEFAULT_VEDIO_COVER "default.png"    // 默认视频封面

    VideoTb* VideoTable; // 数据库类 父类指针
    Json::Value SvConf;  // 服务器的配置文件，因为服务器内函数都是static函数，必须放在类外头

    // 检查端口是否被使用，被使用返回true
    bool IsPortUsed(size_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            _log.fatal("PortCheck","create socket err");
            return true; // error
        }
        // 创建一个socket，判断是否被绑定
        struct sockaddr_in server_addr {};
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port);
        // 绑定测试
        int bind_result = bind(sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
        close(sock);
        // 如果返回值不为0，代表端口已经被使用
        return bind_result != 0; 
    }

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
                    _log.warning(def_name, "missing key '%s' | return status=400", key);
                    return false;
                }
            }
            return true;
        }
        // 检查视频id是否存在，对于一些接口来说很有用
        static bool IsVideoExists(const std::string &def_name, const std::string &video_id, httplib::Response &rsp)
        {
            // 要先查询，判断是否有这个id（mysql执行语句的时候，id不存在并不会报错）
            Json::Value video;
            if (!VideoTable->SelectOne(video_id, &video))
            {
                rsp.status = 404;
                rsp.body = R"({"code":404, "message":"视频id不存在"})";
                rsp.set_header("Content-Type", "application/json");
                _log.error(def_name, "video id not exists! id: [%s]", video_id.c_str());
                return false;
            }
            return true;
        }

        // 将前端发过来的请求指定对应的业务处理接口，转给后端的mysql进行处理
        // 说白了就是把http协议中请求的参数给提取出来，然后调用对应data.hpp中的函数
        // 并给客户端提供正确响应
        // 这里写static是方便将函数映射给httplib
        static void Insert(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.Insert", "post recv from %s", req.remote_addr.c_str());
            static const std::vector<const char *> ins_key = {"name", "info", "video", "cover"};
            // 使用循环来判断文件中是否含有这些字段，没有就返回400
            if (!ReqKeyCheck("Server.Insert", ins_key, req, rsp))
                return;
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
                _log.warning("Server.Insert", "empty video name or video content");
                return;
            }
            // 视频简介为空/视频封面为空替换成默认简介
            if (video_info.size() == 0)
            {
                video_info = DEFAULT_VEDIO_INFO;
                _log.warning("Server.Insert", "empty video info, using default");
            }
            // 拼接视频路径
            std::string root = SvConf["root"].asString();
            // 视频名中包含时间和视频文件的名字，对于本项目而言，安秒记录的时间已经能保证视频文件路径的唯一性了
            // 如果是请求量更大的项目，可以考虑使用毫秒级时间戳，或视频文件的md5字符串作为视频的路径名
            std::string video_path = SvConf["video_root"].asString() + GetTimeStr(NAME_TIME_FORMAT) + video.filename;
            std::string image_path = SvConf["image_root"].asString() + GetTimeStr(NAME_TIME_FORMAT) + image.filename;
            // 将文件写入path
            if (!FileUtil(root + video_path).SetContent(video.content))
            {
                rsp.status = 500; // 服务端出错
                rsp.body = R"({"code":500, "message":"视频文件存储失败"})";
                rsp.set_header("Content-Type", "application/json");
                _log.error("Server.Insert", "write video file err! path:%s", video_path.c_str());
                return;
            }
            if (!FileUtil(root + image_path).SetContent(image.content))
            {
                rsp.status = 500; // 服务端出错
                rsp.body = R"({"code":500, "message":"视频封面图片文件存储失败"})";
                rsp.set_header("Content-Type", "application/json");
                _log.error("Server.Insert", "write cover image file err! path:%s", video_path.c_str());
                return;
            }
            _log.info("Server.Insert", "write file video:'%s' cover:'%s'", video_path.c_str(), image_path.c_str());
            Json::Value video_json;
            video_json["name"] = video_name;
            video_json["info"] = video_info;
            video_json["video"] = video_path;
            video_json["cover"] = image_path;
            // 注意json的键值不能出错，否则会抛出异常（异常处理太麻烦了）
            if (!VideoTable->Insert(video_json))
                return MysqlErrHandler("Server.Insert", rsp);
            // 上传成功
            // rsp.status = 200;
            // rsp.body = R"({"code":0, "message":"上传视频成功"})";
            // rsp.set_header("Content-Type", "application/json");
            _log.info("Server.Insert", "database insert finished! path:%s", video_path.c_str());
            rsp.set_redirect("/index.html", 303); // 将用户重定向到主页
            return;
        }
        // 更新视频，暂时只支持更新视频标题和简介
        static void Update(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.Update", "put recv from %s", req.remote_addr.c_str());
            // static const std::vector<const char *> upd_key = {"name", "info"};
            // 错误，这里不应该检查有没有对应键值，因为请求是通过axjx传过来的，并不是form表单传入
            // 使用循环来判断文件中是否含有这些字段，没有就返回400
            // if (!ReqKeyCheck("Server.Update", upd_key, req, rsp))
            //     return;
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.Update", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            // 检查视频id是否存在
            if (!IsVideoExists("Server.Update", video_id, rsp))
                return;
            // 直接序列化body就行了
            // // 取出对应内容
            // httplib::MultipartFormData name = req.get_file_value("name"); // 视频名称
            // httplib::MultipartFormData info = req.get_file_value("info"); // 视频简介
            // 获取视频标题和简介的内容
            // std::string video_name = name.content;
            // std::string video_info = info.content;
            Json::Value video;
            // video["name"] = video_name;
            // video["info"] = video_info;
            JsonUtil::UnSerialize(req.body,&video);//反序列化
            if (!VideoTable->Update(video_id, video))
                return MysqlErrHandler("Server.Update", rsp);
            // 更新成功
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"更新视频标题/简介成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.Update", "database insert finished! id: [%s]", video_id.c_str());
            return;
        }
        // 删除视频，同样也是通过视频id
        static void Delete(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.Delete", "delete recv from %s", req.remote_addr.c_str());
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.Delete", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            if (!IsVideoExists("Server.Delete", video_id, rsp))
                return;
            // 视频id存在，删除
            // 1.删除本地文件
            Json::Value video;
            if (!VideoTable->SelectOne(video_id, &video))
                return MysqlErrHandler("Server.Delete", rsp);
            // 本地文件删除失败也不要紧，主要是得删除掉数据库中的数据
            FileUtil(SvConf["root"].asString() + video["cover"].asString()).DeleteFile();
            FileUtil(SvConf["root"].asString() + video["video"].asString()).DeleteFile();
            // 2.删除数据库信息
            if (!VideoTable->Delete(video_id))
                return MysqlErrHandler("Server.Delete", rsp);
            // 删除成功
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"删除成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.Delete", "database delete finished! id: [%s]", video_id.c_str());
            return;
        }

        // 通过视频id获取视频
        static void GetOne(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.GetOne", "get recv from %s", req.remote_addr.c_str());
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.GetOne", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            if (!IsVideoExists("Server.GetOne", video_id, rsp))
                return;
            Json::Value video;
            if (!VideoTable->SelectOne(video_id, &video))
                return MysqlErrHandler("Server.GetOne", rsp);

            JsonUtil::Serialize(video, &rsp.body);
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.GetOne", "get success! id: [%s]", video_id.c_str());
        }
        // 通过视频id获取视频d的点赞信息
        static void GetOneView(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.GetOneView", "get recv from %s", req.remote_addr.c_str());
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.GetOneView", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            if (!IsVideoExists("Server.GetOneView", video_id, rsp))
                return;
            Json::Value video;
            if (!VideoTable->SelectVideoView(video_id, &video,true))
                return MysqlErrHandler("Server.GetOneView", rsp);

            JsonUtil::Serialize(video, &rsp.body);
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.GetOneView", "get success! id: [%s]", video_id.c_str());
        }
        // 通过视频路径获取所有视频，或者用query参数来进行搜搜
        static void GetAll(const httplib::Request &req, httplib::Response &rsp)
        {
            // /video 或 /video?s="关键字"
            bool search_flag = false; // 默认所有查询
            std::string search_key;
            if (req.has_param("s")) // 判断是否有s的query参数
            {
                search_flag = true; // 模糊匹配查询
                search_key = req.get_param_value("s");
            }
            Json::Value videos;
            if (!search_flag)
            {
                if (!VideoTable->SelectAll(&videos))
                {
                    return MysqlErrHandler("Server.GetAll",rsp);
                }
                _log.info("Server.GetAll","Select All success");
            }
            else
            {
                if (!VideoTable->SelectLike(search_key, &videos))
                {
                    return MysqlErrHandler("Server.GetAll.Like",rsp);
                }
                _log.info("Server.GetAll.Like","Select Like success! key: [%s]",search_key.c_str());
            }
            JsonUtil::Serialize(videos, &rsp.body);
            rsp.set_header("Content-Type", "application/json");
            return;
        }
        // 更新点赞和点踩
        static void UpdateVideoUp(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UpdateVideoUp", "get recv from %s", req.remote_addr.c_str());
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.UpdateVideoUp", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            if (!IsVideoExists("Server.UpdateVideoUp", video_id, rsp))
                return;
            if (!VideoTable->UpdateVideoUpDown(video_id,true))
                return MysqlErrHandler("Server.UpdateVideoUp", rsp);

            rsp.body = R"({"code":0, "message":"更新点赞成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.UpdateVideoUp", "update success! id: [%s]", video_id.c_str());
        }
        static void UpdateVideoDown(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UpdateVideoDown", "get recv from %s", req.remote_addr.c_str());
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.UpdateVideoDown", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            if (!IsVideoExists("Server.UpdateVideoDown", video_id, rsp))
                return;
            if (!VideoTable->UpdateVideoUpDown(video_id,false))
                return MysqlErrHandler("Server.UpdateVideoDown", rsp);

            rsp.body = R"({"code":0, "message":"更新点踩成功"})";
            rsp.set_header("Content-Type", "application/json");
            _log.info("Server.UpdateVideoDown", "update success! id: [%s]", video_id.c_str());
        }

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
            SvConf = conf["web"]; // 赋值web的json格式给服务器，作为服务器配置
            // 根据配置文件中的选择，实例化对应的单例
            if(conf["sql"]["used"].asString() == "mysql"){
                VideoTable = mysql::VideoTbMysql::GetInstance();//获取单例
                _log.info("server init","get instance of VideoTbMysql");
            }
            //else if(conf["sql"]["used"].asString().find("sqlite")!=std::string::npos){
            else{// 如果没有选择mysql就用sqlite
                VideoTable = sqlite::VideoTbSqlite::GetInstance();//获取单例
                _log.info("server init","get instance of VideoTbSqlite");
            }
            _log.info("server init", "init finished");
        }
        // 建立请求与处理函数的映射关系，设置静态资源根目录，启动服务器，
        bool Run()
        {
            // 1.创建本地的资源文件夹
            FileUtil(SvConf["root"].asString()).CreateDirectory(); // 创建根目录文件夹
            std::string root = SvConf["root"].asString();
            std::string video_real_path = root + SvConf["video_root"].asString(); // ./www/video/
            FileUtil(video_real_path).CreateDirectory();                        // 创建文件夹
            std::string image_real_path = root + SvConf["image_root"].asString(); // ./www/image/
            FileUtil(image_real_path).CreateDirectory();
            // 2.调用httplib的接口，映射服务器表
            // 2.1 设置静态资源根目录
            _srv.set_mount_point("/", SvConf["root"].asString());
            // 2.2 添加请求-处理函数映射关系
            //  正则匹配
            //  获取视频的点赞点踩信息，这个应该放在上面避免view被下方其他正则捕获
            _srv.Put("/video/view/up/([A-Za-z0-9]+)", UpdateVideoUp);
            _srv.Put("/video/view/down/([A-Za-z0-9]+)", UpdateVideoDown);
            _srv.Get("/video/view/([A-Za-z0-9]+)", GetOneView);
            // 视频本身的操作
            _srv.Delete("/video/([A-Za-z0-9]+)", Delete);
            _srv.Put("/video/([A-Za-z0-9]+)", Update);
            _srv.Get("/video/([A-Za-z0-9]+)", GetOne);
            _srv.Get("/video", GetAll);
            _srv.Post("/video", Insert);
            // 3.指定端口，启动服务器
            //   绑定之前，先检查端口是否被使用
            if(IsPortUsed(_port)){
                _log.fatal("server run","port %u already been used!",_port);
                return false;
            }
            //   这里必须用0.0.0.0，否则其他设备无法访问
            _srv.listen("0.0.0.0", _port);
            return true;
        }
    };
}

#endif