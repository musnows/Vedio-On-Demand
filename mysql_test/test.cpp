#include "utils.hpp"

// 将double转为string
std::string double2string(const double& d)
{
    
    std::stringstream s_tmp;
    s_tmp << d;  
    std::string s = s_tmp.str();
    return s;
}


//给已有表中新增键值
//因为表中键值已经固定了，所以可以将函数参数设置为表的参数
int add_key_to_stu(MYSQL *mysql,const std::string& name,int age,double score)
{
    // 1.将传入的参数处理为一个完整的sql语句
    // 因为第一个编号参数，配置的是自增，所以需要传入null
    std::string sql_cmd = "insert into ";
    sql_cmd += TABLENAME;
    sql_cmd += " value (null,'";
    sql_cmd+= name;
    sql_cmd+= "',";
    sql_cmd+= std::to_string(age);
    sql_cmd+= ",";
    sql_cmd+= double2string(score);
    sql_cmd+= ");";
    cout << "[INFO] " << sql_cmd << endl;
    
    // 2.执行语句
    int ret = mysql_query(mysql,sql_cmd.c_str());
    if(ret!=0)
    {
        cerr << "[ERR] mysql insert error: " << mysql_error(mysql) << endl; 
    }
    return ret;
}

// 修改已有学生的成绩
int mod_score_in_stu(MYSQL *mysql,const std::string& name,double score)
{
    std::string sql_cmd = "update ";
    sql_cmd += TABLENAME;
    sql_cmd += " set score=";
    sql_cmd += double2string(score);
    sql_cmd += " where name='";
    sql_cmd += name;
    sql_cmd += "';";
    cout << "[INFO] " << sql_cmd << endl;
    
    // 2.执行语句
    int ret = mysql_query(mysql,sql_cmd.c_str());
    if(ret!=0)
    {
        cerr << "[ERR] mysql mod_score error: " << mysql_error(mysql) << endl; 
    }
    return ret;
}

// 删除键值(根据名字)
int del_key_in_stu(MYSQL *mysql,const std::string& name)
{
    std::string sql_cmd = "delete from ";
    sql_cmd += TABLENAME;
    sql_cmd += " where name='";
    sql_cmd += name;
    sql_cmd += "';";
    cout << "[INFO] " << sql_cmd << endl;
    
    // 执行语句
    int ret = mysql_query(mysql,sql_cmd.c_str());
    if(ret!=0)
    {
        cerr << "[ERR] mysql mod_score error: " << mysql_error(mysql) << endl; 
    }
    return ret;
}

// 返回用户的所有信息,name为空返回所有
void get_all_in_stu(MYSQL *mysql,const std::string& name="")
{
    std::string sql_cmd = "select * from ";
    sql_cmd += TABLENAME;
    if(name.size()!=0)
    {
        sql_cmd += " where name='";
        sql_cmd += name;
        sql_cmd += "'";
    }
    sql_cmd += ";";
    cout << "[INFO] " << sql_cmd << endl;

    int ret = mysql_query(mysql,sql_cmd.c_str());
    if(ret!=0)
    {
        cerr << "[ERR] mysql query error: " << mysql_error(mysql) << endl; 
        return ;
    }
    // 获取结果
    MYSQL_RES *res = mysql_store_result(mysql);
    if (res == nullptr) 
    { 
        cerr << "[ERR] mysql store_result error: " << mysql_error(mysql) << endl; 
        return ; 
    }
    
    int row = mysql_num_rows(res); // 行
    int col = mysql_num_fields(res); //列
    printf("%10s%10s%10s%10s\n", "ID", "姓名", "年龄", "成绩"); 
    for (int i = 0; i < row; i++) 
    { 
        MYSQL_ROW row_data = mysql_fetch_row(res); 
        for (int i = 0; i < col; i++) 
        {
            printf("%10s", row_data[i]); 
        }
        printf("\n"); 
    } 
    // 释放结果
    mysql_free_result(res);
}

int main()
{   
    // 连接数据库
    // 初始化
    MYSQL * mysql = mysql_init(nullptr);
    if (mysql == nullptr) // 返回值为空代表init失败
    { 
        cerr << "[ERR] init mysql handle failed!\n"; 
        return -1; 
    }
    // 连接
    cout << "[INFO] connect to " << HOST << ":" << PORT << " " << USER << " " << DBNAME << endl;
    // 第一个参数为输出型参数。返回值为MYSQL的起始地址，如果错误返回NULL
    if (mysql_real_connect(mysql, HOST, USER, PASSWD, DBNAME, PORT, nullptr, 0) == nullptr) 
    {
        cerr << "[ERR] mysql connect error: " << mysql_error(mysql) << endl; 
        return -1;
    }
    // 配置为和数据库同步的utf8字符集
    mysql_set_character_set(mysql, "utf8");
    // 到这里就已经成功了
    cout << "[INFO] mysql database connect success!" << endl; 

    // 添加一个数据
    // add_key_to_stu(mysql,"牛爷爷",50,64.6);
    // add_key_to_stu(mysql,"小图图",5,72.8);
    // add_key_to_stu(mysql,"大司马",42,87.3);
    // add_key_to_stu(mysql,"乐迪",32,99);

    // 修改已有数据
    // mod_score_in_stu(mysql,"牛爷爷",70);

    // 删除已有键值
    // del_key_in_stu(mysql,"牛爷爷");

    get_all_in_stu(mysql);
    cout << endl;
    get_all_in_stu(mysql,"乐迪");

    // 关闭连接
    mysql_close(mysql); 
    return 0;
}