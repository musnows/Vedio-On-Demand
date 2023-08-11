#ifndef __MY_LOG__
#define __MY_LOG__ 1

#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <string>

namespace vod
{
// #define LOG_DEBUG 0
// #define LOG_INFO 1
// #define LOG_WARINING 2
// #define LOG_ERROR 3
// #define LOG_FATAL 4

// #define LOG_TIME_DISABLE 0
// #define LOG_TIME_STRING 1
// #define LOG_TIME_STAMP 2
// #define LOG_TIME_BOTH 3

// 日志等级
struct LogType{
    static const size_t Debug = 0;   // 调试
    static const size_t Info = 1;    // 正常信息
    static const size_t Warning = 2; // 警告
    static const size_t Error = 3;   // 错误
    static const size_t Fatal = 4;   // 致命错误
};

// 日志时间选项
struct LogTime{
    static const size_t Disable = 0;//禁用
    static const size_t String = 1; //可读时间
    static const size_t Stamp = 2;  //时间戳
    static const size_t Both = 3;   //可读时间+时间戳
};

    // 获取当前可读时间
    std::string GetTimeStr(const std::string &format_str = "%y-%m-%d %H:%M:%S")
    {
        // static string time_str;//只会被定义一次
        // time_str.resize(80);
        static char time_str[80];
        time_t now = time(nullptr);
        struct tm *info;
        info = localtime(&now);
        // strftime((char*)time_str.c_str(), 80,format_str.c_str(), info);
        strftime(time_str, 80, format_str.c_str(), info);
        return time_str;
    }
    // 获取当前时间戳，传入的参数将会和当前时间戳相加
    time_t GetTimestamp(time_t offset = 0)
    {
        time_t now = time(nullptr);
        return now + offset;
    }

    // 实例化一个简单的logger对象
    class Logger
    {
        // 日志默认等级和日志字符串大小
        #define LOG_DEFAULT_LEVEL LogType::Warning // 默认等级为警告
        #define LOG_SIZE 1024
        // 存放了日志等级的数组，用define的日志等级作为下标来映射对应字符串
        static const char *_log_level[6];
    public:
        // log_level设置基础日志等级，小于此等级的日志不打印；log_size日志中允许打印的最大字符长度；log_time为日志中打印时间的格式
        Logger(size_t log_level = LOG_DEFAULT_LEVEL, size_t log_size = LOG_SIZE, size_t log_time = LogTime::Stamp)
            : _level(log_level),
              _log_size(log_size),
              _log_time(log_time)
        {
            _log_info.resize(log_size);
        }

        // 使用完美转发来讲将参数传给logging函数
        template <typename... Args>
        void debug(const std::string& def_name, const std::string& format, Args &&...args)
        {
            _logging(LogType::Debug, def_name, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void info(const std::string& def_name, const std::string& format, Args &&...args)
        {
            _logging(LogType::Info, def_name, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void warning(const std::string& def_name, const std::string& format, Args &&...args)
        {
            _logging(LogType::Warning, def_name, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void error(const std::string& def_name, const std::string& format, Args &&...args)
        {
            _logging(LogType::Error, def_name, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void fatal(const std::string& def_name, const std::string& format, Args &&...args)
        {
            _logging(LogType::Fatal, def_name, format, std::forward<Args>(args)...);
        }

    private:
        size_t _level;    // 日志等级
        size_t _log_size; // 最大可以写入的字符串个数
        size_t _log_time; // 以何种形式记录日志时间
        std::string _log_info;

        // 将format传入并通过可变参数列表，将多余参数写入一个字符串中
        void _logging(size_t level, const std::string& def_name, const std::string& format, ...)
        {
            assert(level >= LogType::Debug && level <=  LogType::Fatal);
            if (level < _level)// 低于定义的等级，不打印
            { 
                return;
            }
            // 使用c语言的可变参数列表来解包
            va_list ap;
            //va_start(ap, format.c_str());//warning11
            va_start(ap, format); //这里只是需要传入最后一个参数，并不是需要传入char*指针
            // 根据format对缓冲区进行二次扩容，如果有长消息直接通过format传入
            // 这样就避免了log info默认长度太小对日志输出长度的影响
            if(format.size()>_log_info.size()){
                _log_info.resize(format.size() + _log_size);
            }
            // 输出到缓冲区
            vsnprintf((char *)_log_info.c_str(), _log_info.size() - 1, format.c_str(), ap);
            va_end(ap);
            // 根据日志等级选择打印到stderr/stdout
            // 超过了error的日志，要使用stderr打印
            FILE *out = (level >= LogType::Error) ? stderr : stdout;
            // 判断defname是否为空
            const char* _def_name = def_name.empty() ? "unknow" : def_name.c_str(); 
            // 格式化打印到文件中
            if (_log_time == LogTime::Disable)
                fprintf(out, "%s | %u | %s | %s\n", 
                        _log_level[level], 
                        std::this_thread::get_id(),
                        _def_name, 
                        _log_info.c_str());
            else
                fprintf(out, "%s | %s | %u | %s | %s\n",
                        _getLogTime().c_str(),
                        _log_level[level],
                        std::this_thread::get_id(),
                        _def_name,
                        _log_info.c_str());

            fflush(out);//刷新缓冲区
        }
        // 获取日志时间参数
        std::string _getLogTime()
        {
            std::string time_log;
            time_log.resize(40);
            switch (_log_time)
            {
            case LogTime::String:
                sprintf((char *)time_log.c_str(), "%s", GetTimeStr().c_str());
                break;
            case LogTime::Stamp:
                sprintf((char *)time_log.c_str(), "%u", (unsigned int)time(nullptr));
                break;
            case LogTime::Both:
                sprintf((char *)time_log.c_str(), "[%s] %u",
                        GetTimeStr().c_str(),
                        (unsigned int)time(nullptr));
                break;
            default:
                time_log = "";
                break;
            }
            return time_log;
        }
    };
    // 类外初始化全局成员
    const char* Logger::_log_level[] = {"DEBUG", "INFO", "WARINING", "ERROR", "FATAL"};
    // 实例化一个全局对象（用单例的话，调用会非常麻烦，所以暂时不做）
    // 如果问道了，这种log类的对象资源消耗较低，可以使用`饿汉`单例在main之前实例化单例   
    Logger _log(LogType::Info,2048,LogTime::String);
}

#endif