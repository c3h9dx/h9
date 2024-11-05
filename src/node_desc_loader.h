/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-28.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include <confuse.h>
#include <map>
#include <string>
#include <vector>

class NodeDescLoader {
  public:
    struct RegisterDesc {
        std::uint8_t number;
        std::string name;
        std::string type;
        int size;
        bool readable;
        bool writable;
        std::vector<std::string> bits_names;
        std::string description;
    };

    struct NodeDesc {
        std::string name;
        std::string description;
        std::map<std::uint16_t, RegisterDesc> registers;
    };

  private:
    cfg_t* cfg;
    std::map<std::uint16_t, NodeDesc> types;
    std::string _nodes_desc_file;
  public:
    NodeDescLoader();
    ~NodeDescLoader();
    void load_file(const std::string& nodes_desc_file);
    void reload();

    std::string get_node_name_by_type(std::uint16_t type);
    std::string get_node_description_by_type(std::uint16_t type);
    std::map<std::uint16_t, RegisterDesc> get_node_register_by_type(std::uint16_t type);
};
