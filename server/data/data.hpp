#ifndef __MY_DATA__
#define __MY_DATA__
#include "../utils.hpp"

namespace vod{
#define CONF_FILEPATH "./config.json"//日志文件路径
#define VEDIO_INFO_MAX_LEN 4096 //视频简介不能过长
    // 视频数据库父类（抽象类）
    class VideoTb{
    protected:
        std::mutex _mutex; // 使用C++的线程，而不直接使用linux的pthread_mutex
        std::string _video_table; // 视频表名称
        std::string _views_table; // 视频点赞信息表名称
        Json::Value _sql_conf;    // 配置文件
    public:
        VideoTb()
        {
            // 读取配置文件中的表名，赋值
            if(!FileUtil(CONF_FILEPATH).GetContent(&_video_table)){
                _log.fatal("VideoTbSqlite init","table_name read err | abort!");
                abort();
            }
            JsonUtil::UnSerialize(_video_table, &_sql_conf);
            _sql_conf = _sql_conf["sql"];
            _video_table = _sql_conf["table"]["video"].asString();
            _views_table = _sql_conf["table"]["views"].asString();
            _log.info("VideoTb","init _sql_conf & table name");
        }
        //检查视频id是否符合规范（长度必须为8）
        bool check_video_id(const std::string& def_name,const std::string& video_id)
        {
            //数据库中定义的是8位id,不为8都是有问题的
            if(video_id.size()!=8){
                _log.warning(def_name,"id size err | sz:%d",video_id.size());
                return false;
            }
            return true;
        }
        //检查视频简介和名字的长度
        bool check_video_info(const std::string& def_name,const Json::Value& video)
        {
            // 1.视频名称不能为空
            if(video["name"].asString().size()==0){
                _log.warning("Video Update","name size == 0");
                return false;
            }
            // 2.简介不能过长（也应该在前端进行限制）
            else if(video["info"].asString().size()>VEDIO_INFO_MAX_LEN){
                _log.warning("Video Update","info size out of max len!");
                return false;
            }
            return true;
        }
        virtual bool Insert(const Json::Value &video) = 0;
        virtual bool Update(const std::string& video_id, const Json::Value &video) = 0;
        virtual bool Delete(const std::string& video_id) = 0;
        virtual bool SelectAll(Json::Value *video_s) = 0;
        virtual bool SelectOne(const std::string& video_id, Json::Value *video) = 0;
        virtual bool SelectLike(const std::string &key, Json::Value *video_s) = 0;
        virtual bool SelectVideoView(const std::string & video_id,Json::Value * video_view,bool update_view=false) = 0;
        virtual bool UpdateVideoView(const std::string& video_id,size_t video_view) = 0;
        virtual bool UpdateVideoUpDown(const std::string& video_id,bool up_flag = true) = 0;
    };
}
#endif