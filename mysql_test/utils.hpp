#pragma once

#include <iostream>
#include <sstream> // stringstream
#include <string>
#include <vector>
#include <memory> // 智能指针
#include <mysql/mysql.h>
// 因为是写项目，应该避免全部展开std
using std::cout;
using std::cerr;
using std::endl;

//在hello数据库中创建了一个stu_test的表
//create table stu_test(
// -> id int primary key auto_increment,
// -> name varchar(30),
// -> age int,
// -> score decimal(4,2));
#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASSWD ""
#define DBNAME "hello"
#define TABLENAME "stu_test"