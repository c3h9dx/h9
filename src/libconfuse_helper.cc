/*
 * Created by crowx on 02/11/2023.
 *
 */

#include "libconfuse_helper.h"

#include "h9frame.h"
#include <spdlog/spdlog.h>

namespace confuse_helpers {

void cfg_err_func(cfg_t* cfg, const char* fmt, va_list args) {
    char msgbuf[1024];
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    // last_confuse_error_message = msgbuf;
    SPDLOG_ERROR(msgbuf);
    // SPDLOG_ERROR(fmt, args);
    //  Logger::default_log.vlog(Log::Level::WARN, __FILE__, __LINE__, fmt, args);
}

int validate_node_id(cfg_t* cfg, cfg_opt_t* opt) {
    auto ids = cfg_opt_size(opt);

    for (auto idx = 0; idx < ids; ++idx) {
        auto id = cfg_opt_getnint(opt, idx);
        if (id < 0 || id > H9frame::H9FRAME_SOURCE_ID_MAX_VALUE) {
            cfg_error(cfg, "option '%s' in section '%s' must have a value between 0-%d", opt->name, cfg->name, H9frame::H9FRAME_SOURCE_ID_MAX_VALUE);
            return -1;
        }
    }

    return 0;
}

int validate_node_type(cfg_t* cfg, cfg_opt_t* opt) {
    auto type = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
    if (type < 1 || type > 0xffff) {
        cfg_error(cfg, "option '%s' in section '%s' must have a value between 1-%d", opt->name, cfg->name, H9frame::H9FRAME_SOURCE_ID_MAX_VALUE);
        return -1;
    }

    return 0;
}

int validate_node_register_type(cfg_t* cfg, cfg_opt_t* opt) {
    char* type = cfg_opt_getnstr(opt, cfg_opt_size(opt) - 1);

    if (strcmp(type, "uint") == 0)
        return 0;
    else if (strcmp(type, "str") == 0)
        return 0;
    else if (strcmp(type, "bool") == 0)
        return 0;
    else if (strcmp(type, "char") == 0)
        return 0;

    cfg_error(cfg, "invalid value for option '%s' in section '%s': %s", opt->name, cfg->name, type);
    return -1;
}

int validate_node_register_size(cfg_t* cfg, cfg_opt_t* opt) {
    auto size = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);

    if (size < 0 || size > 6 * 8) {
        cfg_error(cfg, "option '%s' in section '%s' must have a value less then %d", opt->name, cfg->name, 6 * 8);
        return -1;
    }

    return 0;
}

int validate_node_type_sec(cfg_t* cfg, cfg_opt_t* opt) {
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    unsigned int type = 0;
    try {
        type = std::stoul(cfg_title(sec));
    }
    catch (std::logic_error& e) {
        cfg_error(cfg, "option '%s' must have a value less then %d", cfg->name, 0xffff);
        return -1;
    }

    if (type < 1 || type > 0xffff) {
        cfg_error(cfg, "option '%s' must have a value less then %d", cfg->name, 0xffff);
        return -1;
    }

    return 0;
}

int validate_node_register_number_sec(cfg_t* cfg, cfg_opt_t* opt) {
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    unsigned int type = 0;
    try {
        type = std::stoul(cfg_title(sec));
    }
    catch (std::logic_error& e) {
        cfg_error(cfg, "option '%s' must have a value between 10-%d", cfg->name, 0xffff);
        return -1;
    }

    if (type < 10 || type > 0xff) {
        cfg_error(cfg, "option '%s' must have a value between 10-%d", cfg->name, 0xffff);
        return -1;
    }

    return 0;
}

} // namespace confuse_helpers