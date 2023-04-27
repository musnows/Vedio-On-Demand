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
// 日志等级
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARINING 2
#define LOG_ERROR 3
#define LOG_FATAL 4
// 日志默认等级和日志字符串大小
#define LOG_DEFAULT_LEVEL LOG_WARINING
#define LOG_MAX_SIZE 1024
// 日志时间选项
#define LOG_TIME_DISABLE  0
#define LOG_TIME_STRING  1
#define LOG_TIME_STAMP 2
#define LOG_TIME_BOTH 3

    // 存放了日志等级的数组，用define的日志等级作为下标来映射对应字符串
    const char *log_level[] = {"DEBUG", "INFO", "WARINING", "ERROR", "FATAL"};
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
    // 实例化一个简单的logger对象
    class Logger
    {
    public:
        Logger(size_t log_level = LOG_DEFAULT_LEVEL, size_t log_size = LOG_MAX_SIZE,size_t log_time=LOG_TIME_STAMP)
            : _level(log_level),
              _log_size(log_size),
              _log_time(log_time)
        {
            _log_info.resize(log_size);
        }

        void debug(const char *def_name, const char *format, ...)
        {
            // 获取可变参数列表
            va_list ap; // ap -> char*
            va_start(ap, format);
            // 可以理解为是将可变参数以字符串形式写入logInfo数组中
            vsnprintf((char *)_log_info.c_str(), _log_size - 1, format, ap);
            va_end(ap); // ap = NULL

            _logging(LOG_DEBUG, def_name, _log_info);
        }
        // def_name 调用的函数名字，加上方便查错
        void info(const char *def_name, const char *format, ...)
        {
            // 获取可变参数列表
            va_list ap; // ap -> char*
            va_start(ap, format);
            // 可以理解为是将可变参数以字符串形式写入logInfo数组中
            vsnprintf((char *)_log_info.c_str(), _log_size - 1, format, ap);
            va_end(ap); // ap = NULL

            _logging(LOG_INFO, def_name, _log_info);
        }

        void warning(const char *def_name, const char *format, ...)
        {
            // 获取可变参数列表
            va_list ap;
            va_start(ap, format);
            vsnprintf((char *)_log_info.c_str(), _log_size - 1, format, ap);
            va_end(ap);

            _logging(LOG_WARINING, def_name, _log_info);
        }

        void error(const char *def_name, const char *format, ...)
        {
            // 获取可变参数列表
            va_list ap;
            va_start(ap, format);
            vsnprintf((char *)_log_info.c_str(), _log_size - 1, format, ap);
            va_end(ap);

            _logging(LOG_ERROR, def_name, _log_info);
        }

        void fatal(const char *def_name, const char *format, ...)
        {
            // 获取可变参数列表
            va_list ap;
            va_start(ap, format);
            vsnprintf((char *)_log_info.c_str(), _log_size - 1, format, ap);
            va_end(ap);

            _logging(LOG_FATAL, def_name, _log_info);
        }

    private:
        size_t _level;    // 日志等级
        size_t _log_size; // 最大可以写入的字符串个数
        size_t _log_time; // 以何种形式记录日志时间
        std::string _log_info;

        // 将format传入并通过可变参数列表，将多余参数写入一个字符串中
        void _logging(size_t level, const char *def_name, const std::string &log_info)
        {
            assert(level >= LOG_DEBUG && level <= LOG_FATAL);
            if (level < _level)
            { // 低于定义的等级，不打印
                return;
            }
            // 根据日志等级选择打印到stderr/stdout
            // 超过了error的日志，要使用stderr打印
            FILE *out = (level >= LOG_ERROR) ? stderr : stdout;
            def_name = def_name == nullptr ? "unknow" : def_name; // 判断defname是否为空
            // 格式化打印到文件中
            if(_log_time==LOG_TIME_DISABLE)
                fprintf(out, "%s | %s | %s\n",
                            log_level[level],
                            def_name,
                            log_info.c_str());
            else
                fprintf(out, "%s | %s | %s | %s\n",
                        _getLogTime().c_str(),
                        log_level[level],
                        def_name,
                        log_info.c_str());
        }
        //获取日志时间参数
        std::string _getLogTime()
        {
            std::string time_log;
            time_log.resize(40);
            switch (_log_time)
            {
            case LOG_TIME_STRING:
                sprintf((char*)time_log.c_str(),"%s",GetTimeStr().c_str());
                break;
            case LOG_TIME_STAMP:
                sprintf((char*)time_log.c_str(),"%u",(unsigned int)time(nullptr));
                break;
            case LOG_TIME_BOTH:
                sprintf((char*)time_log.c_str(),"[%s] %u",
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
}

#endif