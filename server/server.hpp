#ifndef __VOD_SERVER__
#define __VOD_SERVER__
#include "httplib.h"
#include "data/mysql.hpp"
#include "data/sqlite3.hpp"
#include "email/mail.h"

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
#define EMAIL_VERIFY_TIME  900 // 邮箱验证有效时间为15分钟
#define EMAIL_VERIFY_INTERVAL  60 // 邮箱验证吗发送间隔为60秒
#define EMAIL_VERIFY_HTML_PATH "./server/email/email_verify.html"
#define USER_COOKIE_KEY "vod_sid" // 用户的cookie标识

    VideoTb* VideoTable; // 数据库类 父类指针
    Json::Value SvConf;  // 服务器的配置文件，因为服务器内函数都是static函数，必须放在类外头
    Json::Value EmailConf;  // 邮箱的配置文件，因为服务器内函数都是static函数，必须放在类外头
    std::unordered_map<std::string,std::pair<std::string,time_t>> EmailVerifyMap; // 用户邮箱和验证码的对应map

    // 检查端口是否被使用，被使用返回true
    bool IsPortUsed(size_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            _log.fatal("PortCheck","create socket err");
            return true; // error
        }
        int optval = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
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
    // 发送邮箱验证码，返回值为是否成功发送
    bool EmailVerifySend(const std::string & target_email,const std::string& verify_code)
    {
        if(verify_code.size() != 6)
        {
            _log.error("EmailVerifySend","verify_code sz != 6"); // 验证码的大小必须严格为6位 
            return false;
        }
        // stmp发送方和密钥
        std::string from = EmailConf["user"].asString();
        std::string passs = EmailConf["passwd"].asString(); //这里替换成自己的授权码
        // 标题
        std::string subject = "【视频点播系统】邮箱验证码：";
        subject += verify_code;
        // 内容
        std::string msg_body;
        if(!FileUtil(EMAIL_VERIFY_HTML_PATH).GetContent(&msg_body)) // 获取html文件内容
        {
            _log.error("EmailVerifySend","open email_verify.html failed!"); // 无法打开文件
            return false;
        }
        // 修改邮件内容里面的验证码
        size_t verfiy_start_pos = msg_body.find("{123456}"); // 查询占位符的位置
        if (verfiy_start_pos == std::string::npos) // 无法找到占位符
        {
            _log.error("EmailVerifySend","cant find verify_code pos in html"); 
            return false;
        }
        // 替换验证码
        msg_body.replace(verfiy_start_pos, 8, verify_code); // 将pos位置往后8位替换成目标验证码

        std::vector<std::string> send_to; //发送列表
        send_to.push_back(target_email); // 发送的目标用户

        SimpleSslSmtpEmail m_ssl_mail(EmailConf["stmp"].asString(),EmailConf["port"].asString()); // 服务器使用ssl
        SmtpBase *base = &m_ssl_mail;
        // 发送
        if(base->SendEmail(from, passs,send_to,subject,msg_body,std::vector<std::string>(),std::vector<std::string>()) != 0)
        {
            _log.error("EmailVerifySend","send email failed!"); // 发送失败
            return false;
        }
        return true;
    }
    // 获取一个特定的cookie字段，如果没有找到，返回空
    std::string GetCookieValue(const std::string& cookie, const std::string& key) 
    {
        std::size_t start = cookie.find(key + "=");
        if (start == std::string::npos) {
            return ""; // Cookie 中没有找到指定的键
        }

        start += key.length() + 1; // 跳过键和等号的长度，指向值的起始位置

        std::size_t end = cookie.find(';', start); // 查找分号，标识值的结束位置
        if (end == std::string::npos) {
            end = cookie.length(); // 若未找到分号，则整个字符串都是值
        }

        return cookie.substr(start, end - start); // 提取值并返回
    }
    // 从req中获取session_id;
    // 如果有cookie但sid为空，代表cookie中不包含session id
    // 返回值：<请求是否拥有cookie,session_id> 
    std::pair<bool,std::string> GetSessionIdFromCookie(const httplib::Request &req)
    {
        bool has_cookie = false;
        std::string session_id="";
        if (req.has_header("Cookie")) 
        {
            has_cookie = true;
            std::string cookie_str = req.get_header_value("Cookie");
            session_id = GetCookieValue(cookie_str,USER_COOKIE_KEY);
        }
        return {has_cookie,session_id};
    }
    // 从req中取出cookie，检查session id是否有效，并设置响应
    // - need_redirect：如果需要将用户重定向到主页，设置此参数为true
    bool SessionCheckHandler(const httplib::Request &req, httplib::Response &rsp,Json::Value* user_info,bool need_redirect = false)
    {
        auto sid_ret = GetSessionIdFromCookie(req);
        if (sid_ret.first) // 有cookie
        {
            std::string session_id = sid_ret.second;
            // 没有找到session_id 或者 长度不匹配
            if(session_id == "" || session_id.size() != SESSION_ID_SIZE){ 
                return false;
            }
            // 获取到了，检查是否有效
            Json::Value user_info;
            // 这里面是session过期了
            if(!VideoTable->UserSessionCheck(session_id,&user_info))
            {
                rsp.status = 503;
                rsp.body = R"({"code":503, "message":"获取session_id信息失败或sessionid失效"})";
                std::string cookie_str = USER_COOKIE_KEY;
                cookie_str += "=0; path=/";
                rsp.set_header("Set-Cookie",cookie_str);
                if(need_redirect){
                    rsp.set_header("Location","/"); // 重定向到主页
                }
                return false;
            }
            // 有效
            return true;
        }
        return false;
    }

    // 服务器端
    class Server
    {
    private:
        size_t _port;         // 服务器监听端口
        httplib::Server _srv; // 用于搭建http服务器
        static Json::Value _temp_user; // 用于session校验时候传值，不实际使用
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
            // 检查用户cookie
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.Insert","session time out");
                return; // sid过期了，直接跳出
            }
            // 需要检查的新视频字段
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
                _log.error("Server.Insert", "write cover image file err! path:%s", image_path.c_str());
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
            // 检查用户cookie
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.Update","session time out");
                return; // sid过期了，直接跳出
            }
            // 错误，这里不应该检查有没有对应键值，因为请求是通过axjx传过来的，并不是form表单传入。参数是在body的json里面
            // static const std::vector<const char *> upd_key = {"name", "info"};
            // if (!ReqKeyCheck("Server.Update", upd_key, req, rsp))
            //     return;
            std::string video_id = req.matches[1];                                   // 从匹配的正则中获取到视频id
            _log.info("Server.Update", "video id recv! id: [%s]", video_id.c_str()); // 将id括起来可以看出来是否有空格
            // 检查视频id是否存在
            if (!IsVideoExists("Server.Update", video_id, rsp))
                return;
            // 直接序列化body就行了
            Json::Value video;
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
            // 检查用户cookie
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.Delete","session time out");
                return; // sid过期了，直接跳出
            }
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
            // 处理用户的cookie
            if (req.has_header("Cookie")) {
                std::string cookie_str = req.get_header_value("Cookie");
                std::string session_id = GetCookieValue(cookie_str,USER_COOKIE_KEY);
                // std::cout << USER_COOKIE_KEY << " = " << session_id << std::endl;
            }
            // 判断是否有s的query参数，如果有代表是在搜素
            if (req.has_param("s")) 
            {
                search_flag = true; // 模糊匹配查询
                search_key = req.get_param_value("s");
            }
            Json::Value videos;
            if (!search_flag) // 不是搜索单个，获取全部
            {
                if (!VideoTable->SelectAll(&videos))
                {
                    return MysqlErrHandler("Server.GetAll",rsp);
                }
                _log.info("Server.GetAll","Select All success");
            }
            else // 模糊匹配
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
            // 检查用户cookie
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.UpdateVideoUp","session time out");
                return; // sid过期了，直接跳出
            }
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
            // 检查用户cookie
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.UpdateVideoDown","session time out");
                return; // sid过期了，直接跳出
            }
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

        static void UserRegister(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UserRegister", "get recv from %s", req.remote_addr.c_str());
            // 检查用户cookie。如果sid过期了，虽然清除sid，但是不做跳出处理
            if(!SessionCheckHandler(req,rsp,&_temp_user)){
                _log.info("Server.UserRegister","session time out | clear it");
            }
            httplib::MultipartFormData name = req.get_file_value("username");   // 用户昵称
            httplib::MultipartFormData email = req.get_file_value("useremail");   // 用户邮箱
            httplib::MultipartFormData avatar = req.get_file_value("useravatar"); // 用户头像
            httplib::MultipartFormData email_verify = req.get_file_value("useremail_verify"); // 用户邮箱验证码
            httplib::MultipartFormData password1 = req.get_file_value("password1"); // 用户密码1
            httplib::MultipartFormData password2 = req.get_file_value("password2"); // 用户密码2
            rsp.status = 400; // 客户端出错
            rsp.set_header("Content-Type", "application/json");
            // 1.两次输入的密码不同
            if(password1.content != password2.content)
            {
                rsp.body = R"({"code":400, "message":"两次输入的密码不匹配"})";
                _log.warning("Server.UserRegister", "two password not match");
                return;
            }
            // 2.邮箱没有在map里面找到，代表没有发送验证码
            auto verify_temp = EmailVerifyMap.find(email.content);
            if(verify_temp == EmailVerifyMap.end())
            {
                rsp.body = R"({"code":400, "message":"未匹配到邮箱信息，请重新获取验证码"})";
                _log.warning("Server.UserRegister", "cant find email '%s' in map",email.content.c_str());
                return;
            }
            // 3.找到了，看看超时没有
            if(GetTimestamp() > ((verify_temp->second).second + EMAIL_VERIFY_TIME))
            {
                rsp.body = R"({"code":400, "message":"邮箱验证码超时失效"})";
                _log.warning("Server.UserRegister", "email '%s' verify out of time",email.content.c_str());
                return;
            }
            // 4.验证码没有超时，看看对不对
            if((verify_temp->second).first != email_verify.content)
            {
                rsp.body = R"({"code":400, "message":"邮箱验证码错误"})";
                _log.warning("Server.UserRegister", "email '%s' verify code err",email.content.c_str());
                return;
            }
            // 走到这里代表验证码正确！将这个邮箱从map中删除
            EmailVerifyMap.erase(verify_temp->first);
            // 创建用户信息json
            Json::Value user;
            user["name"] = name.content;
            user["email"] = email.content;
            // 保存用户头像
            std::string root = SvConf["root"].asString();
            // 图片计算sha256值作为文件路径
            std::string image_path = SvConf["avatar_root"].asString() + HashUtil::String2SHA256(avatar.content) + FileUtil(avatar.filename).GetFileExtension(); 
            // 将文件存到本地
            if (!FileUtil(root + image_path).SetContent(avatar.content))
            {
                rsp.status = 500; // 服务端出错
                rsp.body = R"({"code":500, "message":"视频封面图片文件存储失败"})";
                _log.error("Server.UserRegister", "write avatar image file err! path:%s", image_path.c_str());
                return;
            }
            user["avatar"] = image_path; // 存放文件路径
            user["passwd"] = password1.content; // 用户密码
            // 写入数据库
            if (!VideoTable->UserCreate(user))
                return MysqlErrHandler("Server.UserRegister", rsp);
            // 用户注册成功！
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"用户注册成功"})";
            _log.info("Server.UserRegister", "UserRegister success! [%s]", email.content.c_str());
            return ;
        }
        // 邮箱验证
        static void UserEmailVerify(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UserEmailVerify", "get recv from %s", req.remote_addr.c_str());
            rsp.set_header("Content-Type", "application/json");
            httplib::MultipartFormData email = req.get_file_value("useremail");   // 用户邮箱
            std::string user_email = email.content; // 邮箱
            // 1.先判断map里面有没有这个邮箱，如果有，判断时间间隔
            auto email_it = EmailVerifyMap.find(user_email);
            if(email_it != EmailVerifyMap.end())
            {
                // 没有超过时间间隔，不能发送邮箱
                if((GetTimestamp() - ((email_it->second).second)) <= EMAIL_VERIFY_INTERVAL)
                {
                    rsp.status = 403;
                    rsp.body = R"({"code":403, "message":"邮箱验证码发送频率限制！"})";
                    _log.error("Server.UserEmailVerify", "rate limit for email verify [%s]", user_email.c_str());
                    return;
                }
            }
            // 2.判断邮箱格式合法性：没有找到@或者没有找到. 代表邮箱非法
            if(user_email.find('@') == user_email.npos || user_email.find('.') == user_email.npos) 
            {
                rsp.status = 400;
                rsp.body = R"({"code":400, "message":"邮箱格式不正确"})";
                _log.error("Server.UserEmailVerify", "Incorrect email format [%s]", user_email.c_str());
                return;
            }
            // 3.邮箱格式正确，尝试发送验证码邮件
            //  1.先获取一个验证码
            std::string verify_code = HashUtil::GenerateRandomString(6,"0123456789");
            //  2.写入map
            EmailVerifyMap.insert({user_email,{verify_code,GetTimestamp()}});
            //  3.发送邮件
            if(!EmailVerifySend(user_email,verify_code))
            {   
                rsp.status = 503;
                rsp.body = R"({"code":503, "message":"发送邮件失败"})";
                _log.error("Server.UserEmailVerify", "send email failed [%s]", user_email.c_str());
                return;
            } 
            // 4.发送成功，返回正确
            rsp.status = 200;
            rsp.body = R"({"code":0, "message":"邮箱验证码发送成功"})";
            _log.info("Server.UserEmailVerify", "send email verify success! [%s]", email.content.c_str());
        }
        // 登录
        static void UserLogin(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UserLogin", "get recv from %s", req.remote_addr.c_str());
            rsp.set_header("Content-Type", "application/json");
            httplib::MultipartFormData email = req.get_file_value("useremail");   // 用户邮箱
            httplib::MultipartFormData password = req.get_file_value("password");   // 用户密码
            // 1.先查询这个邮箱是否存在，并验证密码
            Json::Value user;
            _log.debug("Server.UserLogin","enter passwd check");
            if(!VideoTable->UserPasswdCheck(email.content,password.content,&user))
            {
                rsp.status = 400;
                rsp.body = R"({"code":400, "message":"用户不存在或密码错误"})";
                _log.error("Server.UserLogin", "email not found or err", email.content.c_str());
                return;
            }
            // 2.用户存在，返回基本信息给前端
            // 删除密码
            user.removeMember("passwd_salt");
            user.removeMember("passwd_md5");
            // 序列化用户信息作为body
            JsonUtil::Serialize(user, &rsp.body);
            // 获取sid
            std::string session_id;
            if(!VideoTable->UserSessionGet(user["id"].asUInt(),req.remote_addr,&session_id))
            {
                rsp.status = 503;
                rsp.body = R"({"code":503, "message":"获取session_id失败"})";
                _log.error("Server.UserLogin", "get session id failed [%s]", user["email"].asCString());
                return;
            }
            // 设置状态码和cookie
            rsp.status = 200;
            std::string cookie_str = USER_COOKIE_KEY;
            cookie_str += "=";
            cookie_str += session_id;
            cookie_str += "; path=/";
            rsp.set_header("Set-Cookie",cookie_str);
            _log.info("Server.UserLogin", "login success [%s]", email.content.c_str());
        }
        // session有效性检查
        static void UserInfo(const httplib::Request &req, httplib::Response &rsp)
        {
            _log.info("Server.UserInfo", "get recv from %s", req.remote_addr.c_str());
            // 设置响应头
            rsp.set_header("Content-Type", "application/json");
            rsp.status = 403; // 暂时认为不行
            // 判断header是否拥有cookie
            auto sid_ret = GetSessionIdFromCookie(req);
            if (sid_ret.first) 
            {
                std::string session_id = sid_ret.second;
                if(session_id == "" || session_id.size() != SESSION_ID_SIZE) // 没有找到session id
                {
                    rsp.body = R"({"code":403, "message":"cookie中不存在有效session_id"})";
                    _log.warning("Server.UserInfo", "cant get session_id in cookie or session_id size not match");
                }
                // 获取到了，检查是否有效
                Json::Value user_info;
                if(!VideoTable->UserSessionCheck(session_id,&user_info))
                {
                    rsp.status = 503;
                    rsp.body = R"({"code":503, "message":"获取session_id信息失败或sessionid失效"})";
                    _log.error("Server.UserInfo", "get session id failed [%s]", session_id.c_str());
                    return;
                }
                // 有效
                rsp.status = 200;
                // rsp.body = R"({"code":0, "message":"session_id有效"})";
                try{
                    Json::Value res_json;
                    res_json["code"] = 0;
                    res_json["message"] = user_info;
                    JsonUtil::Serialize(res_json, &rsp.body);
                    _log.info("Server.UserInfo", "good session_id");
                    return ;
                }
                catch(...)
                {
                    rsp.status = 503;
                    rsp.body = R"({"code":503, "message":"未知错误"})";
                    _log.debug("Server.UserInfo","err!!!!");
                }
            }
            else // 没有cookie
            { 
                rsp.body = R"({"code":403, "message":"cookie不存在"})";
                _log.warning("Server.UserInfo", "cant get cookie in headers");
            }
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
            EmailConf = conf["email"]; // stmp配置
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
            // 用户相关
            _srv.Post("/usr/login", UserLogin);
            _srv.Post("/usr/register", UserRegister);
            _srv.Post("/usr/email/verify", UserEmailVerify); // 发送验证邮件
            _srv.Get("/usr/info", UserInfo); // sid有效性检查

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
    Json::Value Server::_temp_user;// 类外初始化
}

#endif