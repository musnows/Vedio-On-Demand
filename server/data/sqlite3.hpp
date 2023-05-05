#ifndef __MY_DATA_SQLITE__
#define __MY_DATA_SQLITE__
#include "data.hpp"
#include <mutex>
#include <ctime>
#include <cstdlib>
#include <sqlite3.h>

namespace vod
{
namespace sqlite{
// 视频数据表
#define VIDEO_TABLE_CREATE_SQLITE "CREATE TABLE IF NOT EXISTS tb_video(\
id TEXT(8) UNIQUE NOT NULL DEFAULT (lower((hex(randomblob(4))))),\
name TEXT NOT NULL,\
info TEXT,\
video TEXT NOT NULL,\
cover TEXT NOT NULL,\
insert_time TIMESTAMP DEFAULT (datetime('now', '+8 hours')));"
// 视频点赞信息数据表
#define VIEWS_TABLE_CREATE_SQLITE "create table IF NOT EXISTS tb_views(\
id TEXT(8) NOT NULL,\
up int NOT NULL DEFAULT 0,\
down int NOT NULL DEFAULT 0,\
view int NOT NULL DEFAULT 0);"

    // 回调函数，留空 (因为执行插入/更新等操作时这个函数不会被调用)
    static int SqliteCallback(void *NotUsed, int argc, char **argv, char **azColName)
    {
        _log.info("SqliteCallback","do nothing");
        return 0;
    }
    // 执行sqlite3的语句
    static bool SqliteQuery(sqlite3*db,const std::string& sql)
    {
        // 执行sql语句
        char* zErrMsg;
        int ret = sqlite3_exec(db, sql.c_str(), SqliteCallback, 0, &zErrMsg);
        if (ret != SQLITE_OK)
        {
            _log.error("SqliteQuery.Err","sql: %s",sql.c_str());
            _log.error("SqliteQuery.Err","err: %s",zErrMsg);
            sqlite3_free(zErrMsg);
            return false;
        }
        _log.info("SqliteQuery.Success","sql: %s",sql.c_str());
		return true;
    }

    // 初始化sqlite3
    static sqlite3 *SqliteInit(const std::string& conf_path)
    {
        sqlite3 * db;
        //读取配置文件
        std::string tmp_str;
        Json::Value conf;
        if(!FileUtil(conf_path).GetContent(&tmp_str)){
            //保证读取配置文件不要出错
            _log.fatal("SqliteInit","get config err");
            return nullptr;
        }
        JsonUtil::UnSerialize(tmp_str, &conf);
        // 打开数据库文件
        tmp_str = conf["sql"]["database"].asString()+".db";
        if (sqlite3_open(tmp_str.c_str(), &db))
        {
            _log.fatal("SqliteInit","can't open database: %s\n", sqlite3_errmsg(db));
            return nullptr;
        }
        _log.info("SqliteInit","database open success");
        // 创建两个数据表
        if(!SqliteQuery(db,VIDEO_TABLE_CREATE_SQLITE)){
            _log.fatal("SqliteInit", "sqlite VIDEO_TABLE_CREATE_SQLITE failed!");
            return nullptr;
        }
        if(!SqliteQuery(db,VIEWS_TABLE_CREATE_SQLITE)){
            _log.fatal("SqliteInit", "sqlite VIEWS_TABLE_CREATE_SQLITE failed!");
            return nullptr;
        }
        _log.info("SqliteInit","tables create success");
        return db;
    }
    // 销毁sqlite连接
    static void SqliteDestroy(sqlite3 *db){
        if (db != nullptr) {
            sqlite3_close(db);
            _log.info("SqliteDestroy","destory finished");
            return;
        }
        _log.warning("SqliteDestroy","sqlite3 pointer == nullptr");
    }

    // 视频数据库类
    class VideoTbSqlite :public VideoTb
    {
    private:
        sqlite3 *_db;     // 一个对象就是一个客户端，管理一张表
        std::mutex _mutex; // 使用C++的线程，而不直接使用linux的pthread
        std::string _video_table; // 视频表名称
        std::string _views_table; // 视频点赞信息表名称
        static VideoTbSqlite* _vtb_ptr; // 单例类指针
        
        // 完成mysql句柄初始化
        VideoTbSqlite()
        {
            _db = SqliteInit(CONF_FILEPATH);
            //初始化失败直接abort
            if(_db == nullptr){
                _log.fatal("VideoTbSqlite init","sqlite init failed | abort!");
                abort();
            }
            // 读取配置文件中的表名，赋值
            Json::Value conf;
            if(!FileUtil(CONF_FILEPATH).GetContent(&_video_table)){
                _log.fatal("VideoTbSqlite init","table_name read err | abort!");
                abort();
            }
            JsonUtil::UnSerialize(_video_table, &conf);
            _video_table = conf["sql"]["table"]["video"].asString();
            _views_table = conf["sql"]["table"]["views"].asString();
            _log.info("VideoTbSqlite init","init success");
        }
        // 取消拷贝构造和赋值重载
        VideoTbSqlite(const VideoTbSqlite& _v) = delete;
        VideoTbSqlite& operator==(const VideoTbSqlite& _v)= delete;
    public:
        // 释放msyql操作句柄
        ~VideoTbSqlite(){
            SqliteDestroy(_db);
        }
        // 获取单例(懒汉)
        static VideoTbSqlite* GetInstance()
        {
            if (_vtb_ptr == nullptr)
            {
                _vtb_ptr = new VideoTbSqlite;
            }
            return _vtb_ptr;
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
            return SqliteQuery(_db,sql);//执行语句
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
            return SqliteQuery(_db,sql);//执行语句
        }
        // 删除-传入视频id
        bool Delete(const std::string& video_id)
        {
            if(!check_video_id("Video Delete",video_id))return false;
            #define DELETE_VIDEO "delete from %s where id='%s';"
            std::string sql;
            sql.resize(1024);//扩容
            sprintf((char*)sql.c_str(),DELETE_VIDEO,_video_table.c_str(),video_id.c_str());
            return SqliteQuery(_db,sql);//执行语句
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
            bool ret_status = true;//返回值
            char **pazResult = nullptr,*errMsg; // 二维指针数组，存储查询结果
            int nRow = 0, nColumn = 0;      // 获得查询结果的行数和列数
            _mutex.lock();
            if(sqlite3_get_table(_db,sql.c_str(), &pazResult, &nRow, &nColumn, &errMsg) != SQLITE_OK)
            {
                _log.error("Video SelectAll","sqlite3 get table failed! | err: %s",errMsg);
                ret_status = false;
            }
            else
            {
                int index = nColumn;//从第二列开始，跳过第一行（第一行都是字段名）
                for (int i = 0; i < nRow; i++)
                {
                    Json::Value video;//单个视频
                    for (int j = 0; j < nColumn; j++)
                    {
                        // 前nColumn个数据都是字段名，所以可以用 pazResult[j] 来作为字段名
                        video[pazResult[j]] = pazResult[index]; // 存入数据
                        index++;
                    }
                    // json list
                    video_s->append(video);
                    
                }
                _log.info("Video SelectAll","select all finished");
            }
            sqlite3_free_table(pazResult);
            sqlite3_free(errMsg);
            _mutex.unlock();
            
            return ret_status;
        }
        // 查询单个-输入视频id，输出信息
        bool SelectOne(const std::string& video_id, Json::Value *video)
        {   
            if(!check_video_id("Video SelectOne",video_id))return false;
            #define SELECT_ONE_VIDEO "select * from %s where id='%s';"
            std::string sql;
            sql.resize(512);
            sprintf((char*)sql.c_str(),SELECT_ONE_VIDEO,_video_table.c_str(),video_id.c_str());
            // 开始查询
            bool ret_status = true;//返回值
            char **pazResult = nullptr,*errMsg; // 二维指针数组，存储查询结果
            int nRow = 0, nColumn = 0;      // 获得查询结果的行数和列数
            _mutex.lock();
            if(sqlite3_get_table(_db,sql.c_str(), &pazResult, &nRow, &nColumn, &errMsg) != SQLITE_OK)
            {
                _log.error("Video SelectOne","sqlite3 get table failed! err: %s",errMsg);
                ret_status = false;
            }
            else
            {
                int index = nColumn;//从第二列开始，跳过第一行（第一行都是字段名）
                for (int i = 0; i < nRow; i++)
                {
                    for (int j = 0; j < nColumn; j++)
                    {
                        // 前nColumn个数据都是字段名，所以可以用 pazResult[j] 来作为字段名
                        (*video)[pazResult[j]] = pazResult[index]; // 存入数据
                        index++;
                    }
                    
                }
                _log.info("Video SelectOne","id '%s' found",video_id.c_str());
            }
            sqlite3_free_table(pazResult);
            sqlite3_free(errMsg);
            _mutex.unlock();
            
            return ret_status;
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
            // 开始查询
            bool ret_status = true;//返回值
            char **pazResult = nullptr,*errMsg; // 二维指针数组，存储查询结果
            int nRow = 0, nColumn = 0;      // 获得查询结果的行数和列数
            _mutex.lock();
            if(sqlite3_get_table(_db,sql.c_str(), &pazResult, &nRow, &nColumn, &errMsg) != SQLITE_OK)
            {
                _log.error("Video SelectAll","sqlite3 get table failed! err: %s",errMsg);
                ret_status = false; //结果错误（不直接退出，在后方统一解锁和返回值）
            }
            else
            {
                int index = nColumn;//从第二列开始，跳过第一行（第一行都是字段名）
                for (int i = 0; i < nRow; i++)
                {
                    Json::Value video;//单个视频
                    for (int j = 0; j < nColumn; j++)
                    {
                        // 前nColumn个数据都是字段名，所以可以用 pazResult[j] 来作为字段名
                        video[pazResult[j]] = pazResult[index]; // 存入数据
                        index++;
                    }
                    // json list
                    video_s->append(video);
                }
                _log.info("Video SelectLike","select like '%s' finished",key.c_str());//key不会过长
            }
            sqlite3_free_table(pazResult);
            sqlite3_free(errMsg);
            _mutex.unlock();
            return ret_status;
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
            size_t new_view = 0;// 新的观看量
            (*video_view)["id"] = video_id; // 先统一设置id
            // 开始查询
            bool ret_status = true,query_ret = true;//返回值
            char **pazResult = nullptr,*errMsg; // 二维指针数组，存储查询结果
            int nRow = 0, nColumn = 0;      // 获得查询结果的行数和列数
            _mutex.lock();
            if(sqlite3_get_table(_db,sql.c_str(), &pazResult, &nRow, &nColumn, &errMsg) != SQLITE_OK)
            {
                _log.error("Video SelectOne","sqlite3 get table failed! err: %s",errMsg);
                ret_status = false;
            }
            else
            {   // 判断情况
                if(nRow == 0)// 没有找到，创建一个
                {
                    _log.info("SelectVideoView","no target id '%s' is found",video_id.c_str());
                    #define INSERT_VIDEO_VIEW "insert into %s (id,up,down,view) values ('%s',0,0,1);"
                    sprintf((char*)sql.c_str(),INSERT_VIDEO_VIEW,_views_table.c_str(),video_id.c_str());
                    (*video_view)["up"] = 0;//初始值都是0
                    (*video_view)["down"] = 0;
                    (*video_view)["view"] = 1;//这里新建的时候，代表用户已经点击进入视频页面了，所以初始值是1
                    query_ret = SqliteQuery(_db,sql);
                }
                else // 找到了，筛选数据
                {   
                    int index = nColumn + 1 ;//从第二列第二个开始，跳过第一行和第二行的id字段
                    // j从1开始，跳过第一个id字段
                    for (int j = 1; j < nColumn; j++)
                    {
                        // 只要不是id的数据，都需要改成int类型
                        // 前nColumn个数据都是字段名，所以可以用 pazResult[j] 来作为字段名
                        (*video_view)[pazResult[j]] = atoi(pazResult[index]);
                        // view的数据提取，需要+1
                        if(!strcmp(pazResult[j],"view")){
                            new_view = atoi(pazResult[index]) +1;
                            (*video_view)[pazResult[j]] = new_view;
                        }
                        index++;
                    }
                    _log.info("SelectVideoView","id '%s' found",video_id.c_str());
                }
            }
            sqlite3_free_table(pazResult);
            sqlite3_free(errMsg);
            _mutex.unlock();
            // 创建表失败了，直接返回
            if(!query_ret)return false;
            // 给view+1
            if(update_view && !UpdateVideoView(video_id,new_view)){
                _log.error("SelectVideoView","id '%s' view update false | %d",video_id.c_str(),new_view);
            }
            return ret_status;
        }
        // view数量+1，传入的view应该是`更新后`的值
        bool UpdateVideoView(const std::string& video_id,size_t video_view){
            if(!check_video_id("Video SelectOne",video_id))return false;
            //更新view
            #define UPDATE_VIDEO_VIEW "update %s set view=%d where id='%s';"
            std::string sql;
            sql.resize(256);
            sprintf((char*)sql.c_str(),UPDATE_VIDEO_VIEW,_views_table.c_str(),video_view,video_id.c_str());
            return SqliteQuery(_db,sql);
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
            return SqliteQuery(_db,sql);
        }
    };
    // 类外初始化为null
    VideoTbSqlite* VideoTbSqlite::_vtb_ptr = nullptr;
}
}
#endif
