/*
 * Created by crowx on 01/11/2023.
 *
 */

#include "dev_loader.h"

#include <spdlog/spdlog.h>

#include "antenna_switch_dev.h"
#include "thunder_switch_dev.h"
#include "libconfuse_helper.h"
#include "node_mgr.h"

DevLoader::DevLoader() {
}

DevLoader::~DevLoader() {
}

void DevLoader::load_file(const std::string& devs_desc_file, NodeMgr* dev_mgr) {
    cfg_opt_t cfg_dev_sec[] = {
        CFG_STR("type", nullptr, CFGF_NONE),
        CFG_INT("node_id", -1, CFGF_NONE),
        CFG_INT("controller_id", -1, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_opts[] = {
        CFG_SEC("dev", cfg_dev_sec, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES | CFGF_KEYSTRVAL),
        CFG_END()};

    cfg = cfg_init(cfg_opts, CFGF_NONE);
    cfg_set_error_function(cfg, confuse_helpers::cfg_err_func);
    cfg_set_validate_func(cfg, "dev|node_id", confuse_helpers::validate_node_id);
    cfg_set_validate_func(cfg, "dev|controller_id", confuse_helpers::validate_node_id);
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
        // if (bus_driver_section) {
        std::string dev_name = cfg_title(dev_section);
        std::string type = cfg_getstr(dev_section, "type");
        int node_id = cfg_getint(dev_section, "node_id");

        SPDLOG_WARN(">>> {} {} {}", dev_name, type, node_id);

        if (node_id < 0)
            continue;

        if (type == "AntennaSwitchDev") {
            dev_mgr->add_dev(new AntennaSwitchDev(dev_name, dev_mgr, node_id));
        }
        else if (type == "ThunderSwitchDev") {
            dev_mgr->add_dev(new ThunderSwitchDev(dev_name, dev_mgr, {0}));
        }
    }
}
