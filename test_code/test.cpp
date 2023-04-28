#include "../utils.hpp"
#include <ctime>

// 文件类测试
void FileTest()
{
    //如果文件路径不存在，创建文件
	vod::FileUtil("./www").CreateDirectory();
    //写入测试内容
	vod::FileUtil("./www/index.html").SetContent("<html>124124</html>");
    //尝试获取文件内容
	std::string body;
	vod::FileUtil("./www/index.html").GetContent(&body);
	std::cout << body << std::endl;
    // 获取文件大小
	std::cout << "file size: " << vod::FileUtil("./www/index.html").Size() << std::endl;
}


void JsonTest()
{
	Json::Value val;
	val["姓名"] = "小张";
	val["年龄"]= 18;
	val["性别"] = "男";
	val["成绩"].append(77.5);
	val["成绩"].append(87.5);
	val["成绩"].append(97.5);

	std::string body;
	vod::JsonUtil::Serialize(val, &body);
	std::cout << body << std::endl << std::endl;

	Json::Value stu;
	vod::JsonUtil::UnSerialize(body, &stu);
	std::cout << stu["姓名"].asString() << std::endl;
	std::cout << stu["性别"].asString() << std::endl;
	std::cout << stu["年龄"].asString() << std::endl;
	for (auto &a : stu["成绩"]) {
		std::cout << a.asFloat() << std::endl;
	}
}

void LogTest()
{
    vod::Logger _log(vod::LogType::Info,2048);
    _log.debug("test","%s","debug_Test");
    _log.info("test","%s","this in info");
    _log.warning("test","%s %d","this is warning",333);
    _log.error("test","%s","this is err");
    _log.fatal("test","%s","this is fatal!!!");
}

void LogErrTest()
{
	Json::Value stu;
	std::string str;
	//str = "{\"学习\":[\"语文\",\"EN\"]";//不合法的json字符串
	vod::FileUtil("./test.json").GetContent(&str);//一个断尾的json文件
	vod::JsonUtil::UnSerialize(str,&stu);//成功报错
}

int main()
{
    //FileTest();
    //JsonTest();
    //LogTest();
	LogErrTest();
    return 0;
}