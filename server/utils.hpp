#ifndef __UTILS__
#define __UTILS__ 1 // 保证这个头文件只被包含一次
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
// sha 256 需要的库
#include <iomanip>
#include <openssl/sha.h>
// 需要注意jsoncpp的安装路径，查看/usr/include目录
// Centos8上安装路径是/usr/include/json
#include <json/json.h>
// 测试在deepin上，jsoncpp的安装路径是/usr/include/jsoncpp/json
// #include <jsoncpp/json/json.h>
#include "mylog.hpp"


namespace vod
{
#define CONF_FILEPATH "./config.json" // 项目配置文件路径
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
                _log.info("FileUtil.Exists","file '%s' is not exists",_path.c_str());
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
        // 获取文件内容，返回值为是否成功读取
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
        // 删除文件
        bool DeleteFile()
        {
            if(!this->Exists())
            {
                return false;
            }
            // remove函数不能用来删除文件夹
            if(remove(_path.c_str())!=0)
            {
                _log.error("FileUtil.DeleteFile","remove failed! path:%s",_path.c_str());
                return false;
            }
            _log.info("FileUtil.DeleteFile","remove success! path:%s",_path.c_str());
            return true;
        }
        // 从文件路径获取文件后缀名
        std::string GetFileExtension() 
        {
            size_t dot_pos = _path.rfind('.');
            if (dot_pos != std::string::npos && dot_pos < _path.length() - 1) 
            {
                return _path.substr(dot_pos + 1);
            }
            return ""; // 如果没有后缀名，则返回空字符串
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

    class HashUtil
    {
    public:
            // 获取字符串的sha256值
        static std::string String2SHA256(const std::string& str) 
        {
            // unsigned char能明确我们要处理的是无符号的一个字节0~255的数据
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256_CTX sha256;
            SHA256_Init(&sha256);
            SHA256_Update(&sha256, str.c_str(), str.size());
            SHA256_Final(hash, &sha256);

            std::stringstream ss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }

            return ss.str();
        }

        // 生成指定位数的随机字符串
        // 如果charset为空，则会在大小写字母和数字中生成随机字符串
        static std::string GenerateRandomString(size_t length = 10,const std::string& charset = "")
        {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));// 初始化随机数
            // 使用static修饰，则这个字符串只会被定义一次（第一次进入这个函数的时候被定义）
            static const std::string random_charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            // 根据参数或者默认的字符集来设置目标字符集
            std::string target_charset = random_charset;
            if(charset != "") // 使用的时候传入了字符集
            {
                target_charset = charset;
            }
            // 开始生成随机数
            std::string result_str;
            for (int i = 0; i < length; i++)
            {
                int random_index = std::rand() % target_charset.size();
                result_str += target_charset[random_index];
            }

            return result_str;
        }

        // 给定用户密码和盐的长度，创建加盐后的密码sha256，密码拼接在盐之后。
        // 返回：加密后的sha256 和 盐
        static std::pair<std::string,std::string> EncryptUserPasswd(const std::string & pass, size_t salt_length = 16)
        {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
            // 获取一个盐
            std::string random_str = GenerateRandomString(salt_length);
            // 将盐和用户密码拼接计算sha256
            std::string hash_str = String2SHA256(random_str + pass);
            return {hash_str,random_str};
        }

        // 给定密码和盐，返回加密后的字符串
        static std::string EncryptUserPasswd(const std::string & pass, std::string salt)
        {
            // 将盐和用户密码拼接
            salt += pass;
            // 计算sha256
            return String2SHA256(salt);
        }

        // 获取一个文件的sha256
        static std::string File2SHA256(const std::string & file_path)
        {
            std::string temp;
            if(!FileUtil(file_path).GetContent(&temp)){
                return ""; // 出现错误直接返回空
            }
            return String2SHA256(temp);
        }

    };
}

#endif
