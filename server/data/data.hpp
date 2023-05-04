#ifndef __MY_DATA__
#define __MY_DATA__
#include "../utils.hpp"

namespace vod{
    // 视频数据库父类（抽象类）
    class VideoTb{
    public:
        virtual bool Insert(const Json::Value &video) = 0;
        virtual bool Update(const std::string& video_id, const Json::Value &video) = 0;
        virtual bool Delete(const std::string& video_id) = 0;
        virtual bool SelectAll(Json::Value *video_s) = 0;
        virtual bool SelectOne(const std::string& video_id, Json::Value *video) = 0;
        virtual bool SelectLike(const std::string &key, Json::Value *video_s) = 0;
        virtual bool SelectVideoView(const std::string & video_id,Json::Value * video_view,bool update_view=false) = 0;
        virtual bool UpdateVideoView(const std::string& video_id,size_t video_view) = 0;
        virtual bool UpdateVideoUpDown(const std::string& video_id,bool up_flag = true) = 0;
    };
}
#endif