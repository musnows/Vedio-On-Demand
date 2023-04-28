create table tb_video(
    id VARCHAR(8) NOT NULL DEFAULT (substring(UUID(), 1, 8)) comment '视频id',
    name VARCHAR(50) comment '视频标题',
    info text comment '视频简介',
    video VARCHAR(255) comment '视频链接',
    cover VARCHAR(255) comment '视频封面链接',
    insert_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP comment '视频创建时间'
);