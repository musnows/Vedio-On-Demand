#include "../utils.hpp"
#include "../data.hpp"
#include <ctime>
using namespace std;

// 文件类测试
void FileTest()
{
	// 如果文件路径不存在，创建文件
	vod::FileUtil("./www").CreateDirectory();
	// 写入测试内容
	vod::FileUtil("./www/index.html").SetContent("<html>124124</html>");
	// 尝试获取文件内容
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
	val["年龄"] = 18;
	val["性别"] = "男";
	val["成绩"].append(77.5);
	val["成绩"].append(87.5);
	val["成绩"].append(97.5);

	std::string body;
	vod::JsonUtil::Serialize(val, &body);
	std::cout << body << std::endl
			  << std::endl;

	Json::Value stu;
	vod::JsonUtil::UnSerialize(body, &stu);
	std::cout << stu["姓名"].asString() << std::endl;
	std::cout << stu["性别"].asString() << std::endl;
	std::cout << stu["年龄"].asString() << std::endl;
	for (auto &a : stu["成绩"])
	{
		std::cout << a.asFloat() << std::endl;
	}
}

void LogTest()
{
	vod::Logger _log(vod::LogType::Info, 2048);
	_log.debug("test", "%s", "debug_Test");
	_log.info("test", "%s", "this in info");
	_log.warning("test", "%s %d", "this is warning", 333);
	_log.error("test", "%s", "this is err");
	_log.fatal("test", "%s", "this is fatal!!!");
}

void LogErrTest()
{
	Json::Value stu;
	std::string str;
	// str = "{\"学习\":[\"语文\",\"EN\"]";//不合法的json字符串
	vod::FileUtil("./test.json").GetContent(&str); // 一个断尾的json文件
	vod::JsonUtil::UnSerialize(str, &stu);		   // 成功报错
}

void MysqlTest()
{
	vod::VideoTb testable;
	Json::Value video;
	// video["name"] = "比亚迪仰望u9";
	// video["info"] = "比亚迪仰望u9的宣传视频和cg渲染图，超帅！";
	// // video["video"] = "/video/yangwangu9.mp4";
	// // video["cover"] = "/img/yangwangu9.jpg";
	// // testable.Insert(video);
	// //testable.Update("90182b88",video);
	// testable.Delete("0124a42523");

	// 关键词搜索
	// testable.SelectLike("仰",&video);
	// for(auto&v:video)
	// {
	// 	printf("%s | %s | %s | %s | %s\n",v["id"].asCString(),
	// 									v["name"].asCString(),
	// 									v["info"].asCString(),
	// 									v["video"].asCString(),
	// 									v["cover"].asCString());
	// }

	// id查找
	// if(testable.SelectOne("90182b88",&video))
	// 	printf("%s | %s | %s | %s | %s\n",video["id"].asCString(),
	// 									video["name"].asCString(),
	// 									video["info"].asCString(),
	// 									video["video"].asCString(),
	// 									video["cover"].asCString());

	testable.SelectVideoView("d41d3849", &video, true);
	testable.UpdateVideoUpDown("d41d3849");
	testable.UpdateVideoUpDown("d41d3849", false);
}

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

// 每当有结果返回的时候，用这个函数来处理结果
// 第一个参数可以从sqlite3_exec中主动传入
// argc 是结果的行数（二维数组行数）
// argv 是存放数据的二维数组
// azColName 是存放字段名称的二维数组
static int callback(void *json_videos, int argc, char **argv, char **azColName)
{
	Json::Value *video_s = (Json::Value *)json_videos; // 转为原本的类型
	Json::Value video;								   // 单个视频
	for (int i = 0; i < argc; i++)
	{
		video[azColName[i]] = argv[i] ? argv[i] : "NULL"; // 存入数据
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	video_s->append(video); // 插入到json数组中
	return 0;
}
// sqlite3数据库打开测试
void SqliteTest()
{
	sqlite3 *db; // 数据库指针
	char *zErrMsg = 0;
	std::string sql;
	int ret;

	// 打开数据库文件
	if (sqlite3_open("test.db", &db))
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(0);
	}
	else
	{
		fprintf(stderr, "Opened database successfully\n");
	}
	// 创建数据库
	// std::string sql = R"(CREATE TABLE tb_video(
	// 			id TEXT UNIQUE NOT NULL DEFAULT (hex(randomblob(4))),
	// 			name TEXT  NOT NULL,
	// 			info TEXT ,
	// 			video TEXT  NOT NULL,
	// 			cover TEXT  NOT NULL,
	// 			insert_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
	// 		);)";

	// 插入数据
	// sql = "insert into tb_video (name, info, video, cover) values ('名字2','说明信息2','test2','testc2');";

	// 查询
	Json::Value videos;
	char **pazResult; /* 二维指针数组，存储查询结果 */
	/* 获得查询结果的行数和列数 */
	int nRow = 0, nColumn = 0;
	sqlite3_get_table(db, "SELECT * FROM tb_video;", &pazResult, &nRow, &nColumn, NULL);

	std::cout << nRow <<" " <<  nColumn << std::endl;
	int index = nColumn;//从第二列开始，跳过第一行（第一行都是字段名）
	for (int i = 0; i < nRow; i++)
	{
		Json::Value video;
		for (int j = 0; j < nColumn; j++)
		{
			// 前nColumn个数据都是字段名，所以可以用 pazResult[j] 来打印
			printf("%-8s : %-8s\n", pazResult[j],pazResult[index]);
			video[pazResult[j]] = pazResult[index] ?pazResult[index] : "NULL"; // 存入数据
			index++;
		}
		// json list
		videos.append(video);
	}

	std::string json_str;
	vod::JsonUtil::Serialize(videos, &json_str);
	std::cout << json_str << std::endl;
	
	sqlite3_free_table(pazResult);
	sqlite3_close(db);
}

int main()
{
	// FileTest();
	// JsonTest();
	// LogTest();
	// LogErrTest();
	// MysqlTest();
	SqliteTest();

	return 0;
}