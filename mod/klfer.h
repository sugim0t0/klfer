#ifndef _KLFER_H_
#define _KLFER_H_

#include <asm/io.h>
#include <asm/ioctl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>

#include "klfer_api.h"

/* Module name / version */
#define KLFER_MOD_NAME    "klfer"
#define KLFER_MOD_VERSION "0.4"

#define KLFER_OK           0
#define KLFER_ERR         -1

#define MINOR_BASE         0
#define MINOR_NUM          1

#define MAX_REG_FUNCS      16

#define MAX_LOGS           1024

struct klfer_reg_func
{
    struct kretprobe      krp;
    char                  func_name[MAX_STR_LEN];
    bool                  b_registered;
};

struct klfer_log
{
    struct timespec       time;
    int                   func_idx;
    char                  event_id;
};

struct klfer_mod_data
{
    int                   major_num;
    struct class          *pclass;
    struct device         *pdev;
    struct cdev           chrdev;
    struct klfer_reg_func funcs[MAX_REG_FUNCS];
    struct klfer_log      *logs;
    int                   num_of_funcs;
    int                   num_of_logs;
    atomic_t              open_available;
    bool                  b_logging;   // Logger enable / disable
    bool                  b_jit_log;   // JIT print log enable / disable
    bool                  b_timestamp; // Timestamp enable / disable
    char                  timestamp_fmt;
};

#endif /* _KLFER_H_ */

/**
 * @file  klfer.h
 * @brief klfer header file
 */
