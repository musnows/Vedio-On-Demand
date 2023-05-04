-- sqlite3中不支持uuid函数，所以需要用randomblob函数生成一个随机数，再用hex转成16进制作为视频的id
-- sqlite3默认的时间是utc，所以需要用datetime函数将其转化为东八区的时间
CREATE TABLE IF NOT EXISTS tb_video(
    id TEXT(8) UNIQUE NOT NULL DEFAULT (lower((hex(randomblob(4))))),
    name TEXT NOT NULL,
    info TEXT,
    video TEXT NOT NULL,
    cover TEXT NOT NULL,
    insert_time TIMESTAMP DEFAULT (datetime('now', '+8 hours'))
);
create table IF NOT EXISTS tb_views(
    id TEXT(8) NOT NULL,
    up int NOT NULL DEFAULT 0,
    down int NOT NULL DEFAULT 0,
    view int NOT NULL DEFAULT 0
);
-- 删除表
drop table tb_video;
-- 插入数据
insert into tb_video (name, info, video, cover) values ('名字1','说明信息1','test1','testc1');
-- 查看所有字段
select * from tb_video;
select * from tb_video where id='45f78a68'; 
-- 删除数据
delete from tb_video where id = 'D81382A8';
