# Vedio-On-Demand

VOD System Base On C++


### 1.简介

使用 jsoncpp\httplib\mysql 等C++第三方库实现的视频点播系统。

提供简单的前端页面，实现了如下功能：

* 视频上传/删除
* 视频简介修改
* 视频播放
* 视频点击量统计
* 视频点赞点踩
* ...

> 由于本人主学后端，只对前端语法有基本了解。本项目所用前端页面，由网络上所找视频播放站点的[前端模板](http://www.cssmoban.com/cssthemes/11519.shtml)修改而来。

### 2.依赖项

项目可运行于 `CentOS 8`，依赖项如下

* jsoncpp\httplib 第三方库
* mariadb 数据库
* 用于连接 mariadb\mysql 数据库的 C++ dev 包

```
MariaDB数据库版本
Server version: 10.3.28-MariaDB MariaDB Server
Sqlite3数据库版本
3.26.0 2018-12-01 12:34:55 bf8c1b2b7a5960c282e543b9c293686dccff272512d08865f4600fb58238alt1
Gcc编译器版本
gcc version 8.5.0 20210514 (Red Hat 8.5.0-4) (GCC) 
```

### 3.ToDo

- [ ] data.hpp 中的数据类进行单例封装
- [ ] 提供sqlite3数据库选项