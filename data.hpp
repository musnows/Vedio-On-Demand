#ifndef __MY_DATA__
#define __MY_DATA__
#include "utils.hpp"
#include <mutex>
#include <cstdlib>
#include <mysql/mysql.h>
namespace vod
{
#define MYSQL_CONF_FILEPATH "./config.json"//日志文件路径
#define VEDIO_INFO_MAX_LEN 4096 //视频简介不能过长
    // 调用初始化操作连接数据库
    static MYSQL *MysqlInit(const std::string& conf_path = "./config.json")
    {
        MYSQL *mysql = mysql_init(nullptr);
        if (mysql == nullptr)
        {
            _log.fatal("mysql init", "mysql init failed!");
            return nullptr;
        }
        //读取配置文件
        std::string tmp_str;
        Json::Value conf;
        if(!FileUtil(conf_path).GetContent(&tmp_str)){
            //保证读取配置文件不要出错
            _log.fatal("mysql init","get mysql config err");
            return nullptr;
        }
        JsonUtil::UnSerialize(tmp_str, &conf);
        //通过配置文件读取mysql的配置项
        if (mysql_real_connect(mysql, conf["mysql"]["host"].asCString(), 
                                        conf["mysql"]["user"].asCString(), 
                                        conf["mysql"]["passwd"].asCString(), 
                                        conf["mysql"]["database"].asCString(), 
                                        conf["mysql"]["port"].asUInt(), 
                                        nullptr, 0) == nullptr)
        {
            _log.fatal("mysql init", "mysql server connect failed!");
            return nullptr;
        }
        mysql_set_character_set(mysql, "utf8");
        _log.info("mysql init", "mysql init success");
        return mysql;
    }
    // 销毁mysql连接
    static void MysqlDestroy(MYSQL *mysql){
        if (mysql != nullptr) {
			mysql_close(mysql);
            _log.info("mysql del ","destory finished");
            return;
		}
        _log.warning("mysql del ","mysql pointer == nullptr");
    }
    // 执行mysql语句，返回值为是否执行成功
    static bool MysqlQuery(MYSQL *mysql, const std::string &sql)
    {
        int ret = mysql_query(mysql, sql.c_str());
        // 语句执行失败
		if (ret != 0) {
            _log.error("mysql query","sql: %s",sql.c_str());
            _log.error("mysql query","err: %s",mysql_error(mysql));
			return false;
		}
        _log.info("mysql query right","sql: %s",sql.c_str());
		return true;
    }
    // 视频数据库
    class VideoTb
    {
    private:
        MYSQL *_mysql;     // 一个对象就是一个客户端，管理一张表
        std::mutex _mutex; // 使用C++的线程，而不直接使用linux的pthread
        std::string _table_name;// 视频表名称
    public:
        // 完成mysql句柄初始化
        VideoTb()
        {
            _mysql = MysqlInit(MYSQL_CONF_FILEPATH);
            //初始化失败直接abort
            if(_mysql ==nullptr){
                _log.fatal("VideoTb init","mysql init failed | abort!");
                abort();
            }
            // 读取表名
            Json::Value conf;
            if(!FileUtil(MYSQL_CONF_FILEPATH).GetContent(&_table_name)){
                _log.fatal("VideoTb init","table_name read err | abort!");
                abort();
            }
            JsonUtil::UnSerialize(_table_name, &conf);
            _table_name = conf["mysql"]["table"]["video"].asString();
            _log.info("VideoTb init","init success");
        }
        // 释放msyql操作句柄
        ~VideoTb(){
            MysqlDestroy(_mysql);
        }
        // 新增-传入视频信息
        bool Insert(const Json::Value &video)
        {
            if(video["name"].asString().size()==0){//1.视频名称不能为空
                _log.warning("Video Insert","name size == 0");
                return false;
            }
            else if(video["info"].asString().size()>VEDIO_INFO_MAX_LEN){//2.简介不能过长（也应该在前端进行限制）
                _log.warning("Video Insert","info size out of max len!");
                return false;
            }
            //插入的sql语句
            #define INSERT_VIDEO "insert into %s (name,info,video,cover) values ('%s','%s','%s','%s');"
            std::string sql;
            sql.resize(2048+video["info"].asString().size());//扩容，避免简介超过预定义长度
            sprintf((char*)sql.c_str(),INSERT_VIDEO,_table_name.c_str(),
                                    video["name"].asCString(),
                                    video["info"].asCString(),
                                    video["video"].asCString(),
                                    video["cover"].asCString());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 修改-传入视频id和新的信息(暂时不支持修改视频封面和路径)
        bool Update(const std::string& video_id, const Json::Value &video)
        {   
            if(video["name"].asString().size()==0){//1.视频名称不能为空
                _log.warning("Video Update","name size == 0");
                return false;
            }
            else if(video["info"].asString().size()>VEDIO_INFO_MAX_LEN){//2.简介不能过长（也应该在前端进行限制）
                _log.warning("Video Update","info size out of max len!");
                return false;
            }
            #define UPDATE_VIDEO_INFO "update %s set name='%s',info='%s' where id='%s';"
            std::string sql;
            sql.resize(2048+video["info"].asString().size());//扩容，避免简介超过预定义长度
            sprintf((char*)sql.c_str(),UPDATE_VIDEO_INFO,_table_name.c_str(),
                                    video["name"].asCString(),
                                    video["info"].asCString(),
                                    video_id.c_str());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 删除-传入视频id
        bool Delete(const std::string& video_id)
        {
            if(video_id.size()==0){
                _log.warning("Video Delete","id size == 0");
                return false;
            }
            #define DELETE_VIDEO "delete from %s where id='%s';"
            std::string sql;
            sql.resize(1024);//扩容
            sprintf((char*)sql.c_str(),DELETE_VIDEO,_table_name.c_str(),video_id.c_str());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 查询所有-输出所有视频信息（视频列表）
        bool SelectAll(Json::Value *videos)
        {
            #define SELET_ALL "select * from %s;"
        }
        // 查询单个-输入视频id，输出信息
        bool SelectOne(int video_id, Json::Value *video);
        // 模糊匹配-输入名称关键字，输出视频信息
        bool SelectLike(const std::string &key, Json::Value *videos);
    };
}
#endif
