#ifndef _KLFER_API_H_
#define _KLFER_API_H_

#include <linux/ioctl.h>
#include <stdbool.h>

#define KLFER_IOC_TYPE ']'
/* Device name */
#define KLFER_DEVICE_NAME "klferdev"

enum klfer_cmd_flag {
    KLFER_REG_FUNC_FLAG = 1,
    KLFER_RESET_FLAG,
    KLFER_SET_PARAMS_FLAG,
    KLFER_DUMP_SETTINGS_FLAG,
    KLFER_DUMP_LOGS_FLAG,
#ifdef DEBUG
    KLFER_SAMPLE_FLAG,
#endif
};

#define MAX_STR_LEN 64
struct klfer_func_cfg {
    char func_name [MAX_STR_LEN];
    bool b_reg; // true: Register, false: Unregister
};

/**
 * Control parameters (int)
 *      3                   2                   1                   0
 *    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | X |                                               | C | B | A |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   Common(A-C):
 *      b0* > Setting is ignored (Keep the current setting)
 *   A: Logger Enable(1) / Disable(0)
 *      b11 > Enable Logger
 *      b10 > Disable Logger
 *   B: JIT (Just-In-Time) print log Enable(1) / Disable(0)
 *      b11 > Enable JIT print log
 *      b10 > Disable JIT print log
 *   C: Timestamp Enable(1) / Disable(0)
 *      b11 > Enable Timestamp
 *      b10 > Disable Timestamp
 *
 *   X: Timestamp format (Setting is ignored if timestamp update flag (Bit(5)) is 0.)
 *      b00 > Absolute time
 *      b01 > Relative time from the first log
 *      b10 > Relative time from the previous log
 */
#define UPDATE_FLAG            0b10
#define VALUE_BIT              0b01
#define PARAM_MASK             (UPDATE_FLAG | VALUE_BIT)
#define ENABLE_PARAM(param, shift) \
                               (param = (param | (PARAM_MASK << shift)))
#define DISABLE_PARAM(param, shift) \
                               (param = ((param & ~(PARAM_MASK << shift)) | (UPDATE_FLAG << shift)))

#define LOGGER_CTRL_SHIFT      0
#define JIT_CTRL_SHIFT         2
#define TIMESTAMP_CTRL_SHIFT   4
#define TIMESTAMP_FMT_SHIFT    30

#define TS_FMT_MASK(param)     ((param >> TIMESTAMP_FMT_SHIFT) & 0b11)
#define TS_FMT_ABS             0b00
#define TS_FMT_RLTV_FIRST      0b01
#define TS_FMT_RLTV_PREV       0b10

/* Macros for parameter setting */
#define ENABLE_LOGGER(param)   ENABLE_PARAM(param, LOGGER_CTRL_SHIFT)
#define DISABLE_LOGGER(param)  DISABLE_PARAM(param, LOGGER_CTRL_SHIFT)
#define ENABLE_JIT(param)      ENABLE_PARAM(param, JIT_CTRL_SHIFT)
#define DISABLE_JIT(param)     DISABLE_PARAM(param, JIT_CTRL_SHIFT)
#define ENABLE_TS(param)       ENABLE_PARAM(param, TIMESTAMP_CTRL_SHIFT)
#define DISABLE_TS(param)      DISABLE_PARAM(param, TIMESTAMP_CTRL_SHIFT)

#define SET_TS_FMT_ABS(param)  (param = (param | (TS_FMT_ABS << TIMESTAMP_FMT_SHIFT)))
#define SET_TS_FMT_RLTV_FIRST(param) \
                               (param = (param | (TS_FMT_RLTV_FIRST << TIMESTAMP_FMT_SHIFT)))
#define SET_TS_FMT_RLTV_PREV(param) \
                               (param = (param | (TS_FMT_RLTV_PREV << TIMESTAMP_FMT_SHIFT)))

/* Commands for ioctl */
#define KLFER_REG_FUNC         _IOW(KLFER_IOC_TYPE, KLFER_REG_FUNC_FLAG,      struct klfer_func_cfg)
#define KLFER_RESET            _IOW(KLFER_IOC_TYPE, KLFER_RESET_FLAG,         NULL)
#define KLFER_SET_PARAMS       _IOW(KLFER_IOC_TYPE, KLFER_SET_PARAMS_FLAG,    int)
#define KLFER_DUMP_SETTINGS    _IOR(KLFER_IOC_TYPE, KLFER_DUMP_SETTINGS_FLAG, NULL)
#define KLFER_DUMP_LOGS        _IOR(KLFER_IOC_TYPE, KLFER_DUMP_LOGS_FLAG,     NULL)
#ifdef DEBUG
#define KLFER_SAMPLE           _IOR(KLFER_IOC_TYPE, KLFER_SAMPLE_FLAG,        NULL)
#endif

#endif /* _KLFER_API_H_ */
