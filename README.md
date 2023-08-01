# Vedio-On-Demand

VOD System Base On C++


## 1.简介

使用 jsoncpp\httplib\mysql 等C++第三方库实现的视频点播系统。

提供简单的前端页面，实现了如下功能：

* 视频上传/删除
* 视频简介修改
* 视频播放
* 视频点击量统计
* 视频点赞点踩
* ...

> 由于本人主学后端，只对前端语法有基本了解。本项目所用前端页面，由网络上所找视频播放站点的[前端模板](http://www.cssmoban.com/cssthemes/11519.shtml)修改而来。

## 2.依赖项

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

### 2.1 安装jsoncpp


```bash
# centos
sudo yum install epel-release 
sudo yum install jsoncpp-devel
# debian
sudo apt-get install libjsoncpp-dev
```

安装完毕之后，请检查你的 `/usr/include` 路径，查看jsoncpp头文件的路径

```
ls /usr/include
```

比如我在CentOS8上安装时，路径为

```
/usr/include/json
```

但在`deepin20.9`上安装时，路径为

```
/usr/include/jsoncpp/json
```

检查路径后，需要修改 [server/utils](./server/utils.hpp) 里对jsoncpp的include

### 2.2 httplib

无需安装，这是一个单头文件的库，已在本项目中包含。

### 2.3 mysql/mariadb

参考我的博客：https://blog.musnow.top/posts/577382991/

在deepin下的安装命令和centos不同，在此记录如下

```bash
# deepin安装mysql开发包
sudo apt install default-libmysqlclient-dev
sudo apt-get install libmariadbclient-dev
```

## 3.ToDo

- [x] data 中的数据类进行单例封装
- [x] 提供 sqlite3 数据库选项
- [ ] 提供 dockerfile 进行 docker 部署