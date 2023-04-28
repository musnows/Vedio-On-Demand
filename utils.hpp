#ifndef __UTILS__
#define __UTILS__ 1 // 保证这个头文件只被包含一次
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
// 需要注意jsoncpp的安装路径，查看/usr/include目录
// 我的安装路径是/usr/include/json
#include <json/json.h>
#include "mylog.hpp"

namespace vod
{
    // 用于处理本地文件的类
    class FileUtil
    {
    private:
        std::string _path; // 文件路径名称
    public:
        // 构造函数
        FileUtil(const std::string &file_path) : _path(file_path)
        {}
        // 判断文件路径中的文件是否存在
        bool Exists()
        {
            // 通过c语言的access函数来判断是否存在文件（注意不是assert）
            // F_OK即代表判断文件是否存在
            // 返回值不为0代表不存在
            int ret = access(_path.c_str(), F_OK);
            if (ret != 0)
            {
                _log.error("FileUtil.Exists","file '%s' is't exists",_path.c_str());
                return false;
            }
            return true;
        } 
        // 获取文件大小
        uint64_t Size()
        {
            //要保证文件存在
            if (this->Exists() == false)
            {
                return 0;
            }
            //
            struct stat st;
            // stat接口用于获取文件属性，其中 st_size就是文件大小
            int ret = stat(_path.c_str(), &st);
            if (ret != 0)
            {
                _log.error("FileUtil.Size","get file '%s' stat failed!",_path.c_str());
                return 0;
            }
            return st.st_size;
        } 
        // 获取文件内容
        bool GetContent(std::string *body)
        {
            std::ifstream ifs;
            ifs.open(_path, std::ios::binary);
            if (ifs.is_open() == false)
            {
                _log.error("FileUtil.GetContent","open file '%s' failed!",_path.c_str());
                return false;
            }
            size_t flen = this->Size();
            body->resize(flen);
            ifs.read(&(*body)[0], flen);
            if (ifs.good() == false)
            {
                _log.error("FileUtil.GetContent","read file '%s' failed!",_path.c_str());
                ifs.close();
                return false;
            }
            ifs.close();
            return true;
        } 
        // 设置文件内容（写入数据）
        bool SetContent(const std::string &body)
        {
            std::ofstream ofs;
            ofs.open(_path, std::ios::binary);
            if (ofs.is_open() == false)
            {
                _log.error("FileUtil.SetContent","open file '%s' failed!",_path.c_str());
                return false;
            }
            ofs.write(body.c_str(), body.size());
            if (ofs.good() == false)
            {
                _log.error("FileUtil.SetContent","write file '%s' failed!",_path.c_str());
                ofs.close();
                return false;
            }
            ofs.close();
            return true;
        } 
        // 新增文件路径（如果文件不存在）
        bool CreateDirectory()
        {
            if (this->Exists())
            {
                return true;
            }
            umask(0);//保证创建出来的文件权限没有问题
            mkdir(_path.c_str(), 0777);
            return true;
        }
    };

    // json序列化和反序列化的方法
    // 这个类没有成员，只是一个方法类
    // static修饰函数，让函数可以通过类名调用
    class JsonUtil
    {
    public:
        // 序列化，body为输出型参数
        // 为了和反序列化的方法入参一直，body采用指针传参而不采用引用
        // 个人感觉用指针更能表明这个是输出型参数
        static bool Serialize(const Json::Value &value, std::string* body)
        {
            Json::StreamWriterBuilder builder;
            std::unique_ptr<Json::StreamWriter> sw(builder.newStreamWriter());

            std::stringstream ss;
            int ret = sw->write(value, &ss);
            if (ret != 0)
            {
                _log.error("JsonUtil.Ser","Serialize failed!");
                _log.error("JsonUtil.Ser body ",body->c_str());//打印出错误的字符串
                return false;
            }
            *body = ss.str();
            return true;
        }
        // 反序列化，此时body为入参，value为输出型参数
        static bool UnSerialize(const std::string &body, Json::Value *value)
        {
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

            std::string err;
            bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), value, &err);
            if (ret == false)
            {
                _log.error("JsonUtil.UnSer","UnSerialize failed!");
                _log.error("JsonUtil.UnSer body",body.c_str());
                return false;
            }
            return true;
        }
    };
}

#endif
