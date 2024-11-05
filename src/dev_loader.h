/*
 * Created by crowx on 01/11/2023.
 *
 */

#pragma once

#include <confuse.h>
#include <string>
#include <map>

class NodeMgr;

class DevLoader {
  public:
    struct DevDesc {
        std::uint8_t type;
    };

  private:
    cfg_t* cfg;
    std::map<std::string, DevDesc> devs;

  public:
    DevLoader();
    ~DevLoader();

    void load_file(const std::string& devs_desc_file, NodeMgr* dev_mgr);
};
