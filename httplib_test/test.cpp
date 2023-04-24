#include <iostream>
#include <string>
#include "../httplib.h"
using std::cout;
using std::cerr;
using std::endl;
using namespace httplib;

#define WEB_ROOT "./web"

// 使用引用进行修改
void get_hello(const Request& req,Response& res)
{
    cout << "[GET] recv: " << req.path << endl;
    res.body = "hello muxue! get recv at ";
    res.body+= req.path;
    res.status = 200;
}

void get_num(const Request& req,Response& res)
{
    cout << "[GET] recv: " << req.path << endl;
    res.body = "hello muxue! get recv at ";
    res.body+= req.path;
    // 客户端请求的时候，会获取到一个文件，内部是请求路径中的数字
    for(auto& e:req.matches){
        cout << e << endl;
    }
    std::string num = req.matches[1];
    res.body = "hello muxue! get recv at ";
    res.body+= req.path;
    res.body+= num;
    //等同于给body添加，并指定contenttype
    //res.set_content(num,"text/plain");
    res.status = 200;
}
void post_file(const Request& req,Response& res)
{
    //判断req中是否有指定名字的文件
    if(req.has_file("file1") == false){
        res.status = 404;
        return ;
    }
    MultipartFormData file = req.get_file_value("file1");
    cout << "[file.name] "<< file.filename << endl;
    cout << "[file.content]\n" << file.content << endl;
    
    std::string body = "您的请求收到了: ";
    body+= file.filename;
    //设置返回体的编码类型，否则中文乱码
    res.set_content(body,"text/plain; charset=utf-8");
    res.status = 200; 
}

int main()
{
    Server server;
    //设置web根目录
    server.set_mount_point("/",WEB_ROOT);

    //添加能够处理的请求办法
    //server.Get("/",get_hello);
    server.Get("/mu",get_hello);
    //正则表达式
    // \d表示数字，+表示匹配前面的字符一次或多次，()表示单独捕捉数据
    server.Get("/numbers",get_hello);
    server.Get("/numbers/(\\d+)",get_num);
    //post请求的处理
    server.Post("/uploads",post_file);
    //监听，开始服务
    cout << "[server] start at 0.0.0.0:50002 " << endl;
    server.listen("0.0.0.0",50002);
    return 0;
}