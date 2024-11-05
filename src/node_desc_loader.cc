/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-28.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#include "node_desc_loader.h"

#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>

#include "spdlog/spdlog.h"
#include "libconfuse_helper.h"


static int cfg_include_wrapper(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv) {
    if (!cfg || !argv) {
        errno = EINVAL;
        return CFG_FAIL;
    }

    if (argv[0][0] == '/') {
        return cfg_include(cfg, opt, argc, argv);
    }


    size_t len = strlen(cfg->filename) + strlen(argv[0]) + 2;
    char *path = (char*)malloc(len);
    if (!path)
        return CFG_FAIL;

//    char *dir_name = dirname(cfg->filename);

    char *tmp = strrchr(cfg->filename, '/');

    if (tmp) {
        size_t l = tmp - cfg->filename;
        stpncpy(path, cfg->filename, l);
        path[l] = '\0';
    }
    else {
        path[0] = '.';
        path[1] = '\0';
    }

    strcat(path, "/");
    strcat(path, argv[0]);
//    size_t len = strlen(dir_name) + strlen(argv[0]) + 2;
//    char *path = (char*)malloc(len);
//    if (!path)
//        return CFG_FAIL;

//    snprintf(path, len, "%s/%s", dir_name, argv[0]);
//    free(dir_name);

    int ret = CFG_FAIL;

    struct stat st;
    int err = stat((const char *)path, &st);
    if ((!err) && S_ISREG(st.st_mode)) {
        argv[0] = path;
        SPDLOG_INFO("Loading nodes description subfile: {}", path);
        ret = cfg_include(cfg, opt, argc, argv);
    }
    else if ((!err) && S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *dir;
            ret = CFG_SUCCESS;
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_type == DT_REG) {
                    char *tmp = strrchr(dir->d_name, '.');
                    if (tmp == NULL || strcmp(tmp, ".conf"))
                        continue;

                    size_t len = strlen(path) + strlen(dir->d_name) + 2;
                    char *file = (char*)malloc(len);
                    if (!file) {
                        ret = CFG_FAIL;
                        break;
                    }

                    snprintf(file, len, "%s/%s", path, dir->d_name);

                    argv[0] = file;
                    SPDLOG_INFO("Loading nodes description subfile: {}", file);
                    ret =  cfg_include(cfg, opt, argc, argv);
                    free(file);

                    if (ret != CFG_SUCCESS) break;
                }
            }
            closedir(d);
        }
    }

    free(path);
    return ret;
}

NodeDescLoader::NodeDescLoader():
    cfg(nullptr) {
}

NodeDescLoader::~NodeDescLoader() {
    if (cfg) {
        cfg_free(cfg);
    }
}

void NodeDescLoader::load_file(const std::string& nodes_desc_file) {
    _nodes_desc_file = nodes_desc_file;

    cfg_opt_t cfg_register_sec[] = {
        CFG_STR("name", nullptr, CFGF_NONE),
        CFG_STR("type", nullptr, CFGF_NONE),
        CFG_INT("size", 0, CFGF_NONE),
//        CFG_STR("mode", nullptr, CFGF_NONE),
        CFG_BOOL("readable", cfg_false, CFGF_NONE),
        CFG_BOOL("writable", cfg_false, CFGF_NONE),
        CFG_STR_LIST("bits-names", (char*)"{}", CFGF_NONE),
        CFG_STR("description", "", CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_type_sec[] = {
        CFG_STR("name", nullptr, CFGF_NONE),
        CFG_STR("description", "", CFGF_NONE),
        CFG_SEC("register", cfg_register_sec, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()};

    cfg_opt_t cfg_opts[] = {
        CFG_SEC("node_type", cfg_type_sec, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_FUNC("include", &cfg_include_wrapper),
        CFG_END()};

    cfg = cfg_init(cfg_opts, CFGF_NONE);
    cfg_set_error_function(cfg, confuse_helpers::cfg_err_func);
    cfg_set_validate_func(cfg, "node_type|register|type", confuse_helpers::validate_node_register_type);
    cfg_set_validate_func(cfg, "node_type|register|size", confuse_helpers::validate_node_register_size);
    cfg_set_validate_func(cfg, "node_type", confuse_helpers::validate_node_type_sec);
    cfg_set_validate_func(cfg, "node_type|register",confuse_helpers::validate_node_register_number_sec);

    int ret = cfg_parse(cfg, nodes_desc_file.c_str());

    if (ret == CFG_FILE_ERROR) {
        SPDLOG_ERROR("Nodes description file ({}) - file error", nodes_desc_file.c_str());
        cfg_free(cfg);
        return;
    }
    else if (ret == CFG_PARSE_ERROR) {
        SPDLOG_ERROR("Nodes description file ({}) - parse error", nodes_desc_file.c_str());
        cfg_free(cfg);
        return;
    }

    int dn = cfg_size(cfg, "node_type");
    for (int di = 0; di < dn; ++di) {
        cfg_t* device = cfg_getnsec(cfg, "node_type", di);
        int device_type = std::stoul(cfg_title(device));

        auto& desc = types[device_type];
        desc.name = std::string(cfg_getstr(device, "name"));
        desc.description = std::string(cfg_getstr(device, "description"));

        int rn = cfg_size(device, "register");
        for (int ri = 0; ri < rn; ++ri) {
            cfg_t* register_c = cfg_getnsec(device, "register", ri);
            int req_num = std::stoul(cfg_title(register_c));

            if (req_num < 10 || req_num > 0xff)
                continue; // skip reg number below 10 ang higher then 255

            auto& reg = desc.registers[req_num];
            reg.number = req_num;
            reg.name = std::string(cfg_getstr(register_c, "name"));
            reg.type = std::string(cfg_getstr(register_c, "type"));
            reg.size = cfg_getint(register_c, "size");
            reg.readable = cfg_getbool(register_c, "readable");
            reg.writable = cfg_getbool(register_c, "writable");
            for (int i = cfg_size(register_c, "bits-names") - 1; i >= 0; --i) {
                reg.bits_names.push_back(cfg_getnstr(register_c, "bits-names", i));
            }
            reg.description = std::string(cfg_getstr(register_c, "description"));
        }
    }
}

void NodeDescLoader::reload() {
    if (cfg) {
        cfg_free(cfg);
    }

    types.clear();

    load_file(_nodes_desc_file);
}

std::string NodeDescLoader::get_node_name_by_type(std::uint16_t type) {
    if (types.count(type)) {
        return types[type].name;
    }
    return "";
}

std::string NodeDescLoader::get_node_description_by_type(std::uint16_t type) {
    if (types.count(type)) {
        return types[type].description;
    }
    return "";
}

std::map<std::uint16_t, NodeDescLoader::RegisterDesc> NodeDescLoader::get_node_register_by_type(std::uint16_t type) {
    if (types.count(type)) {
        return types[type].registers;
    }
    return std::map<std::uint16_t, NodeDescLoader::RegisterDesc>();
}
