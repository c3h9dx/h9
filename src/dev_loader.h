/*
 * Created by crowx on 01/11/2023.
 *
 */

#pragma once

#include <spdlog/spdlog.h>
#include <confuse.h>
#include <string>
#include <map>

class NodeMgr;
class Dev;

class DevLoader {
  public:
    typedef Dev* (*CreteDevFun)(std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes);
    class RegisterDev {
      public:
        RegisterDev(const std::string& type, CreteDevFun create_fun) {
            SPDLOG_INFO("Register dev driver: '{}'.", type);
            DevLoader::devs_create_fun[type] = create_fun;
        }
    };

  private:
    cfg_t* cfg;
    static std::map<std::string, CreteDevFun> devs_create_fun;

    Dev* create_dev(const std::string& type, std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes);
  public:
    void load_file(const std::string& devs_desc_file, NodeMgr* dev_mgr);
};
