/*
 * Created by crowx on 01/11/2023.
 *
 */

#include "dev_loader.h"

#include <spdlog/spdlog.h>

#include <utility>

#include "libconfuse_helper.h"
#include "node_mgr.h"

std::map<std::string, DevLoader::CreteDevFun> DevLoader::devs_create_fun;

Dev* DevLoader::create_dev(const std::string& type, std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes) {
    if (DevLoader::devs_create_fun.count(type)) {
        return DevLoader::devs_create_fun[type](std::move(name), node_mgr, std::move(nodes));
    }
    return nullptr;
}

void DevLoader::load_file(const std::string& devs_desc_file, NodeMgr* dev_mgr) {
    cfg_opt_t cfg_dev_sec[] = {
        CFG_STR("type", nullptr, CFGF_NONE),
        CFG_INT_LIST("node_ids", nullptr, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_opts[] = {
        CFG_SEC("dev", cfg_dev_sec, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES | CFGF_KEYSTRVAL),
        CFG_END()};

    cfg = cfg_init(cfg_opts, CFGF_NONE);
    cfg_set_error_function(cfg, confuse_helpers::cfg_err_func);
    cfg_set_validate_func(cfg, "dev|node_ids", confuse_helpers::validate_node_id);
    int ret = cfg_parse(cfg, devs_desc_file.c_str());

    if (ret == CFG_FILE_ERROR) {
        SPDLOG_ERROR("Nodes description file ({}) - file error", devs_desc_file.c_str());
        cfg_free(cfg);
        return;
    }
    else if (ret == CFG_PARSE_ERROR) {
        SPDLOG_ERROR("Nodes description file ({}) - parse error", devs_desc_file.c_str());
        cfg_free(cfg);
        return;
    }

    int n = cfg_size(cfg, "dev");
    for (int i = 0; i < n; i++) {
        cfg_t* dev_section = cfg_getnsec(cfg, "dev", i);

        std::string dev_name = cfg_title(dev_section);
        std::string type = cfg_getstr(dev_section, "type");

        unsigned int ids_len = cfg_size(dev_section, "node_ids");
        std::vector<std::uint16_t> node_ids(ids_len);

        if (ids_len <= 0) {
            SPDLOG_WARN("Missing nodes for {} dev", dev_name);
        }

        for (int j = 0; j < ids_len; j++) {
            node_ids[j] = cfg_getnint(dev_section, "node_ids", j);
        }

        Dev *tmp = create_dev(type, dev_name, dev_mgr, std::move(node_ids));

        if (tmp) {
            dev_mgr->add_dev(tmp);
        }
    }
}
