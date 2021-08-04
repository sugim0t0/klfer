#ifndef _KLFER_API_H_
#define _KLFER_API_H_

#include <linux/ioctl.h>
#include <stdbool.h>

#define KLFER_IOC_TYPE ']'
/* Device name */
#define KLFER_DEVICE_NAME "klferdev"

enum klfer_cmd_flag {
    KLFER_REG_FUNC_FLAG = 1,
    KLFER_UNREG_FUNC_FLAG,
    KLFER_RESET_FLAG,
    KLFER_ENABLE_LOG_FLAG,
    KLFER_DISABLE_LOG_FLAG,
    KLFER_PRINT_REG_FUNCS_FLAG,
#ifdef DEBUG
    KLFER_SAMPLE_FLAG,
#endif
};

#define MAX_STR_LEN 64
struct klfer_func_cfg {
    char func_name [MAX_STR_LEN];
    char grp_id    [MAX_STR_LEN];
    bool b_rec_timestamp;
};

#define KLFER_NO_COMMAND      -1
 
#define KLFER_REG_FUNC        _IOW(KLFER_IOC_TYPE, KLFER_REG_FUNC_FLAG,        struct klfer_func_cfg)
#define KLFER_UNREG_FUNC      _IOW(KLFER_IOC_TYPE, KLFER_UNREG_FUNC_FLAG,      struct klfer_func_cfg)
#define KLFER_RESET           _IOW(KLFER_IOC_TYPE, KLFER_RESET_FLAG,           unsigned long)
#define KLFER_ENABLE_LOG      _IOW(KLFER_IOC_TYPE, KLFER_ENABLE_LOG_FLAG,      unsigned long)
#define KLFER_DISABLE_LOG     _IOW(KLFER_IOC_TYPE, KLFER_DISABLE_LOG_FLAG,     unsigned long)
#define KLFER_PRINT_REG_FUNCS _IOR(KLFER_IOC_TYPE, KLFER_PRINT_REG_FUNCS_FLAG, unsigned long)
#ifdef DEBUG
#define KLFER_SAMPLE          _IOR(KLFER_IOC_TYPE, KLFER_SAMPLE_FLAG,          unsigned long)
#endif

#endif /* _KLFER_API_H_ */
