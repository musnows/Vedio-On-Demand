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
        bool SelectAll(Json::Value *video_s)
        {
            #define SELET_ALL "select * from %s;"
            std::string sql;
            sql.resize(512);
            sprintf((char*)sql.c_str(),SELET_ALL,_table_name.c_str());
            // 这里加锁是为了保证结果集能被正常报错（并不是防止修改原子性问题,mysql本身就已经维护了原子性）
            // 下方执行语句后，如果不保存结果集 而又执行一次搜索语句，之前搜索的结果就会丢失
            // 加锁是为了保证同一时间只有一个执行流在进行查询操作，避免结果集丢失
            _mutex.lock();
            // 语句执行失败了
            if (!MysqlQuery(_mysql, sql)) {
                _mutex.unlock();
                _log.error("Video SelectAll","query failed");
                return false;
            }
            // 保存结果集到本地
            MYSQL_RES *res = mysql_store_result(_mysql);
            if (res == nullptr) {
                _mutex.unlock();
                _log.error("Video SelectAll","mysql store result failed");
                return false;
            }
            _mutex.unlock();
            int num_rows = mysql_num_rows(res);//获取结果集的行数
            for (int i = 0; i < num_rows; i++) {
                MYSQL_ROW row = mysql_fetch_row(res);//获取每一行的列数
                Json::Value video;
                video["id"] = row[0];
                video["name"] = row[1];
                video["info"] = row[2];
                video["video"] = row[3];
                video["cover"] = row[4];
                //json list
                video_s->append(video);
            }
            mysql_free_result(res);//释放结果集
            _log.info("Video SelectAll","select all finished");
            return true;
        }
        // 查询单个-输入视频id，输出信息
        bool SelectOne(const std::string& video_id, Json::Value *video)
        {
            if(video_id.size()==0){
                _log.warning("Video SelectOne","id size == 0");
                return false;
            }
            #define SELECT_ONE_VIDEO "select * from %s where id='%s';"
            std::string sql;
            sql.resize(512);
            sprintf((char*)sql.c_str(),SELECT_ONE_VIDEO,_table_name.c_str(),video_id.c_str());
            _mutex.lock();
            // 语句执行失败了
            if (!MysqlQuery(_mysql, sql)) {
                _mutex.unlock();
                _log.error("Video SelectOne","query failed");
                return false;
            }
            // 保存结果集到本地
            MYSQL_RES *res = mysql_store_result(_mysql);
            if (res == nullptr) {
                _mutex.unlock();
                _log.error("Video SelectOne","mysql store result failed");
                return false;
            }
            _mutex.unlock();
            int num_rows = mysql_num_rows(res);//获取结果集的行数
            if(num_rows==0){//一行都没有，空空如也
                _log.warning("Video SelectOne","no target id '%s' is found",video_id.c_str());
                return false;
            }
            else if(num_rows>1)//id不唯一，大bug
            {
                _log.fatal("Video SelectOne","id '%s' more than once! num:%d",video_id.c_str(),num_rows);
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            // 这里是调用参数里面的对象的[]重载，所以需要解引用
            (*video)["id"] = video_id;
            (*video)["name"] = row[1];
            (*video)["info"] = row[2];
            (*video)["video"] = row[3];
            (*video)["cover"] = row[4];
            mysql_free_result(res);
            _log.info("Video SelectOne","id '%s' found",video_id.c_str());
            return true;
        }
        // 模糊匹配-输入名称关键字，输出视频信息
        bool SelectLike(const std::string &key, Json::Value *video_s)
        {
            //当时创建数据库的时候，name只能是50个字节
            if(key.size()==0 || key.size()>50){
                _log.warning("Video SelectLike","key size out of range | size:%d",key.size());
                return false;
            }
            #define SELECT_LIKE "select * from %s where name like '%%%s%%';"//模糊匹配
            std::string sql;
            sql.resize(512+key.size());
            sprintf((char*)sql.c_str(),SELECT_LIKE,_table_name.c_str(),key.c_str());
            _mutex.lock();
            // 语句执行失败了
            if (!MysqlQuery(_mysql, sql)) {
                _mutex.unlock();
                _log.error("Video SelectLike","query failed");
                return false;
            }
            // 保存结果集到本地
            MYSQL_RES *res = mysql_store_result(_mysql);
            if (res == nullptr) {
                _mutex.unlock();
                _log.error("Video SelectLike","mysql store result failed");
                return false;
            }
            _mutex.unlock();
            int num_rows = mysql_num_rows(res);//获取结果集的行数
            for (int i = 0; i < num_rows; i++) {
                MYSQL_ROW row = mysql_fetch_row(res);//获取每一行的列数
                Json::Value video;
                video["id"] = row[0];
                video["name"] = row[1];
                video["info"] = row[2];
                video["video"] = row[3];
                video["cover"] = row[4];
                //json list
                video_s->append(video);
            }
            mysql_free_result(res);//释放结果集
            _log.info("Video SelectLike","select like '%s' finished",key.c_str());//key不会过长
            return true;
        }
    };
}
#endif