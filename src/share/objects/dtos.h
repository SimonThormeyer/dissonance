#ifndef SRC_SHARE_OBJECTS_DTOS_
#define SRC_SHARE_OBJECTS_DTOS_

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "share/defines.h"
#include "share/tools/utils/utils.h"
#include <iostream>
#include <utility>
#include <vector>

struct GetPositionInfo {
  public:
    GetPositionInfo() : unit_(-1), pos_({-1, -1}) {}
    GetPositionInfo(int unit) : unit_(unit), pos_({-1, -1}) {}
    GetPositionInfo(position_t pos) : unit_(-1), pos_(pos) {}
    
    // getter
    int unit() const { return unit_; }
    position_t pos() const { return pos_; }

    // methods 
    nlohmann::json ToJson() const {
      if (unit_ != -1) 
        return unit_;
      if (pos_.first != -1)
        return pos_;
      return nlohmann::json();
    }

  private:
    const int unit_;
    const position_t pos_;
};

class Dto {
  public: 
    Dto(std::string command, std::string username) : command_(command), username_(username) { }

    // getter 
    std::string username() { return username_; }
    std::string command() { return command_; }
    virtual std::string return_cmd() { return ""; };
    virtual std::map<int, GetPositionInfo> position_requests() { return {}; }
    
    // setter
    
    // methods
    virtual nlohmann::json ToJson() const {
      return {{"command", command_}, { "username", username_}, {"data", nlohmann::json()}};
    }

  private:
    std::string command_;
    std::string username_;
};


class GetPosition : Dto {
  public: 
    GetPosition(std::string username, std::string return_cmd, 
        std::map<int, GetPositionInfo> position_requests) 
        : Dto("get_positions", username), return_cmd_(return_cmd), position_requests_(position_requests) {}
    GetPosition(nlohmann::json json) 
        : Dto(json["command"], json["username"]), return_cmd_(json["data"]["return_cmd"]) {
      for (const auto& it : json["data"]["position_requests"].get<std::map<std::string, nlohmann::json>>()) {
        if (it.second.is_null())
          position_requests_.insert(std::make_pair(std::stoi(it.first), GetPositionInfo()));
        else if (it.second.is_number())
          position_requests_.insert(std::make_pair(std::stoi(it.first), GetPositionInfo(it.second.get<int>())));
        else 
          position_requests_.insert(std::make_pair(std::stoi(it.first), GetPositionInfo(it.second.get<position_t>())));
      }
    }
    
    // getter    
    std::string return_cmd() { return return_cmd_; };
    std::map<int, GetPositionInfo> position_requests() { return position_requests_; }

    // methods 
    nlohmann::json ToJson() const {
      nlohmann::json json = Dto::ToJson();
      json["data"]["return_cmd"] = return_cmd_;
      json["data"]["position_requests"] = nlohmann::json();
      for (const auto& it : position_requests_)
        json["data"]["position_requests"][std::to_string(it.first)] = it.second.ToJson();
      return json;
    }

  private: 
    const std::string return_cmd_;
    std::map<int, GetPositionInfo> position_requests_;
};

#endif


