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
#define KLFER_MOD_VERSION "0.1"

#define KLFER_OK           0
#define KLFER_ERR         -1

#define MINOR_BASE         0
#define MINOR_NUM          1

#define MAX_REG_FUNCS      16
#define MAX_GRPS           MAX_REG_FUNCS

struct klfer_reg_func
{
    struct kretprobe krp;
    char             func_name[MAX_STR_LEN];
    unsigned int     grp_idx;
    bool             b_rec_timestamp;
    bool             b_registered;
};

struct klfer_grp
{
    char grp_id[MAX_STR_LEN];
    int  num_of_funcs;
    int  cnt;
};

struct klfer_mod_data
{
    int                   major_num;
    struct class          *pclass;
    struct device         *pdev;
    struct cdev           chrdev;
    struct klfer_grp      grps[MAX_GRPS];
    struct klfer_reg_func funcs[MAX_REG_FUNCS];
    atomic_t              open_available;
    bool                  b_logging;
};

#endif /* _KLFER_H_ */

/**
 * @file  klfer.h
 * @brief klfer header file
 */
