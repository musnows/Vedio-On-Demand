#ifndef __MY_DATA__
#define __MY_DATA__
#include "utils.hpp"
#include <mutex>
#include <ctime>
#include <cstdlib>
#include <mysql/mysql.h>
namespace vod
{
#define TIME_DELETA 8 //东八区 
#define CONF_FILEPATH "./config.json"//日志文件路径
#define VEDIO_INFO_MAX_LEN 4096 //视频简介不能过长
    // 调用初始化操作连接数据库
    static MYSQL *MysqlInit(const std::string& conf_path)
    {
        // 配置文件不能为空
        if (conf_path.size()==0){
            _log.fatal("mysql init","conf_path empty");
            return nullptr;
        }
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
            _log.error("mysql query","err[%u]: %s",mysql_errno(mysql),mysql_error(mysql));
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
        std::string _video_table;// 视频表名称
        std::string _views_table; // 视频点赞信息表名称

        //检查视频id是否符合规范
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
            if(video["name"].asString().size()==0){//1.视频名称不能为空
                _log.warning("Video Update","name size == 0");
                return false;
            }
            else if(video["info"].asString().size()>VEDIO_INFO_MAX_LEN){//2.简介不能过长（也应该在前端进行限制）
                _log.warning("Video Update","info size out of max len!");
                return false;
            }
            return true;
        }
    public:
        // 完成mysql句柄初始化
        VideoTb()
        {
            _mysql = MysqlInit(CONF_FILEPATH);
            //初始化失败直接abort
            if(_mysql ==nullptr){
                _log.fatal("VideoTb init","mysql init failed | abort!");
                abort();
            }
            // 读取表名
            Json::Value conf;
            if(!FileUtil(CONF_FILEPATH).GetContent(&_video_table)){
                _log.fatal("VideoTb init","table_name read err | abort!");
                abort();
            }
            JsonUtil::UnSerialize(_video_table, &conf);
            _video_table = conf["mysql"]["table"]["video"].asString();
            _views_table = conf["mysql"]["table"]["views"].asString();
            _log.info("VideoTb init","init success");
        }
        // 释放msyql操作句柄
        ~VideoTb(){
            MysqlDestroy(_mysql);
        }
        // 新增-传入视频信息
        bool Insert(const Json::Value &video)
        {
            if(!check_video_info("Video Insert",video))return false;
            //插入的sql语句
            #define INSERT_VIDEO "insert into %s (name,info,video,cover) values ('%s','%s','%s','%s');"
            std::string sql;
            sql.resize(2048+video["info"].asString().size());//扩容，避免简介超过预定义长度
            sprintf((char*)sql.c_str(),INSERT_VIDEO,_video_table.c_str(),
                                    video["name"].asCString(),
                                    video["info"].asCString(),
                                    video["video"].asCString(),
                                    video["cover"].asCString());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 修改-传入视频id和新的信息(暂时不支持修改视频封面和路径)
        bool Update(const std::string& video_id, const Json::Value &video)
        {   
            if(!check_video_id("Video Update",video_id))return false;
            if(!check_video_info("Video Update",video))return false;
            #define UPDATE_VIDEO_INFO "update %s set name='%s',info='%s' where id='%s';"
            std::string sql;
            sql.resize(2048+video["info"].asString().size());//扩容，避免简介超过预定义长度
            sprintf((char*)sql.c_str(),UPDATE_VIDEO_INFO,_video_table.c_str(),
                                    video["name"].asCString(),
                                    video["info"].asCString(),
                                    video_id.c_str());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 删除-传入视频id
        bool Delete(const std::string& video_id)
        {
            if(!check_video_id("Video Delete",video_id))return false;
            #define DELETE_VIDEO "delete from %s where id='%s';"
            std::string sql;
            sql.resize(1024);//扩容
            sprintf((char*)sql.c_str(),DELETE_VIDEO,_video_table.c_str(),video_id.c_str());
            return MysqlQuery(_mysql,sql);//执行语句
        }
        // 查询所有-输出所有视频信息（视频列表）
        bool SelectAll(Json::Value *video_s)
        {
            #define SELET_ALL "select * from %s;"
            std::string sql;
            sql.resize(512);
            sprintf((char*)sql.c_str(),SELET_ALL,_video_table.c_str());
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
                _log.error("Video SelectAll","mysql store result failed | err[%u]: %s",mysql_errno(_mysql),mysql_error(_mysql));
                return false;
            }
            //_mutex.unlock();
            int num_rows = mysql_num_rows(res);//获取结果集的行数
            for (int i = 0; i < num_rows; i++) {
                MYSQL_ROW row = mysql_fetch_row(res);//获取每一行的列数
                Json::Value video;
                video["id"] = row[0];
                video["name"] = row[1];
                video["info"] = row[2];
                video["video"] = row[3];
                video["cover"] = row[4];
                video["insert_time"] = row[5]; //mysql中存放的就是可读时间 （其实存时间戳更好）
                //json list
                video_s->append(video);
            }
            mysql_free_result(res);//释放结果集
            _mutex.unlock();
            _log.info("Video SelectAll","select all finished");
            return true;
        }
        // 查询单个-输入视频id，输出信息
        bool SelectOne(const std::string& video_id, Json::Value *video)
        {   
            if(!check_video_id("Video SelectOne",video_id))return false;
            #define SELECT_ONE_VIDEO "select * from %s where id='%s';"
            std::string sql;
            sql.resize(512);
            sprintf((char*)sql.c_str(),SELECT_ONE_VIDEO,_video_table.c_str(),video_id.c_str());
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
                _log.error("Video SelectOne","mysql store result failed | err[%u]: %s",mysql_errno(_mysql),mysql_error(_mysql));
                return false;
            }
            //_mutex.unlock();
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
            (*video)["insert_time"] = row[5];
            mysql_free_result(res);
            _mutex.unlock();
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
            sprintf((char*)sql.c_str(),SELECT_LIKE,_video_table.c_str(),key.c_str());
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
                _log.error("Video SelectLike","mysql store result failed | err[%u]: %s",mysql_errno(_mysql),mysql_error(_mysql));
                return false;
            }
            //_mutex.unlock();
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
            _mutex.unlock();
            _log.info("Video SelectLike","select like '%s' finished",key.c_str());//key不会过长
            return true;
        }
        // 查询视频的view（点击量 点赞等等信息）
        // update_view 是否需要更新view计数器
        bool SelectVideoView(const std::string & video_id,Json::Value * video_view,bool update_view=false)
        {
            if(!check_video_id("Video View",video_id))return false;//视频id不符合规范
            std::string sql;
            sql.resize(512);
            // 先查询再插入
            #define SELECT_VIDEO_VIEW "select * from %s where id='%s';"
            sprintf((char*)sql.c_str(),SELECT_VIDEO_VIEW,_views_table.c_str(),video_id.c_str());
            _mutex.lock();
            if(!MysqlQuery(_mysql,sql)){
                _mutex.unlock();
                _log.error("SelectVideoView","query failed");
                return false;
            }
            // 保存结果集到本地
            MYSQL_RES*res=mysql_store_result(_mysql);
            if(res==nullptr){
                _mutex.unlock();
                _log.error("SelectVideoView","mysql store result failed | err[%u]: %s",mysql_errno(_mysql),mysql_error(_mysql));
                return false;
            }
            //_mutex.unlock();
            // 这里是调用参数里面的对象的[]重载，所以需要解引用
            // 先都设置一下返回值里面的视频id
            (*video_view)["id"] = video_id;
            int num_rows=mysql_num_rows(res);//获取行数
            if(num_rows==0){
                // 没有就插入一个
                _log.info("SelectVideoView","no target id '%s' is found",video_id.c_str());
                #define INSERT_VIDEO_VIEW "insert into %s (id,up,down,view) values ('%s',0,0,1);"
                sprintf((char*)sql.c_str(),INSERT_VIDEO_VIEW,_views_table.c_str(),video_id.c_str());
                (*video_view)["up"] = 0;//初始值都是0
                (*video_view)["down"] = 0;
                (*video_view)["view"] = 1;//这里新建的时候，代表用户已经点击进入视频页面了，所以初始值是1
                return MysqlQuery(_mysql,sql);
            }
            else if(num_rows>1){
                _log.fatal("SelectVideoView","id '%s' more than once!",video_id.c_str());//id不唯一，更有问题了
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            (*video_view)["up"] = atoi(row[1]);//全都要转成int
            (*video_view)["down"] = atoi(row[2]);
            size_t new_view = atoi(row[3])+1;
            (*video_view)["view"] = new_view ; // 返回之后需要调用inset再给view+1，所以这里返回值就直接+1了
            mysql_free_result(res);
            _mutex.unlock();
            _log.info("SelectVideoView","id '%s' found",video_id.c_str());
            // 给view+1
            if(update_view && !UpdateVideoView(video_id,new_view)){
                _log.error("SelectVideoView","id '%s' view update false | %d",video_id.c_str(),new_view);
            }
            return true;
        }
        // view数量+1，传入的view应该是`更新后`的值
        bool UpdateVideoView(const std::string& video_id,size_t video_view){
            if(!check_video_id("Video SelectOne",video_id))return false;
            //更新view
            #define UPDATE_VIDEO_VIEW "update %s set view=%d where id='%s';"
            std::string sql;
            sql.resize(256);
            sprintf((char*)sql.c_str(),UPDATE_VIDEO_VIEW,_views_table.c_str(),video_view,video_id.c_str());
            return MysqlQuery(_mysql,sql);
        }
        // up和down的更新
        // up_flag：true更新up / false更新down
        bool UpdateVideoUpDown(const std::string& video_id,bool up_flag = true)
        {
            if(!check_video_id("Video SelectOne",video_id))return false;
            #define UPDATE_VIDEO_UP "update %s set up=%d where id='%s';"
            #define UPDATE_VIDEO_DOWN "update %s set down=%d where id='%s';"
            std::string sql;
            sql.resize(256);
            Json::Value video_view;//视频点赞信息
            if(!SelectVideoView(video_id,&video_view,false)){
                _log.warning("UpdateVideoUpDown","id '%s' select old up/down failed",video_id.c_str());
                return false;
            }
            if(up_flag){
                size_t value = video_view["up"].asUInt() + 1;
                sprintf((char*)sql.c_str(),UPDATE_VIDEO_UP,_views_table.c_str(),value,video_id.c_str());
            }
            else{
                size_t value = video_view["down"].asUInt() + 1;
                sprintf((char*)sql.c_str(),UPDATE_VIDEO_DOWN,_views_table.c_str(),value,video_id.c_str());
            }
            return MysqlQuery(_mysql,sql);
        }
    };
}
#endif
