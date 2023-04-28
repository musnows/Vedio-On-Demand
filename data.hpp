#ifndef __MY_DATA__
#define __MY_DATA__
#include "utils.hpp"
#include <mutex>
#include <cstdlib>
#include <mysql/mysql.h>
namespace vod
{
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
        if (mysql_real_connect(mysql, conf["mysql"]["host"].asString().c_str(), 
                                        conf["mysql"]["user"].asString().c_str(), 
                                        conf["mysql"]["passwd"].asString().c_str(), 
                                        conf["mysql"]["database"].asString().c_str(), 
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
    class VedioTb
    {
    private:
        MYSQL *_mysql;     // 一个对象就是一个客户端，管理一张表
        std::mutex _mutex; // 使用C++的线程，而不直接使用linux的pthread
    public:
        // 完成mysql句柄初始化
        VedioTb()
        {
            _mysql = MysqlInit();
            //初始化失败直接abort
            if(_mysql ==nullptr){
                _log.fatal("VedioTb init","mysql init failed | abort!");
                abort();
            }
        }
        // 释放msyql操作句柄
        ~VedioTb(){
            MysqlDestroy(_mysql);
        }
        // 新增-传入视频信息
        bool Insert(const Json::Value &video);
        // 修改-传入视频id，和信息
        bool Update(int video_id, const Json::Value &video);
        // 删除-传入视频id
        bool Delete(const int video_id);
        // 查询所有-输出所有视频信息（视频列表）
        bool SelectAll(Json::Value *videos);
        // 查询单个-输入视频id,输出信息
        bool SelectOne(int video_id, Json::Value *video);
        // 模糊匹配-输入名称关键字，输出视频信息
        bool SelectLike(const std::string &key, Json::Value *videos);
    };
}
#endif
