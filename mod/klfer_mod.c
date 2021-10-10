/**
 * @file  klfer_mod.c
 * @brief KLFER (Kernel Logger for Function Entries and Returns)
 */

#include "klfer.h"
#ifdef DEBUG
#include "klfer_dbg.h"
#endif

static inline void klfer_unregister_kretprobe(int);
static int  klfer_entry_handler(struct kretprobe_instance *, struct pt_regs *);
static int  klfer_ret_handler(struct kretprobe_instance *, struct pt_regs *);
static int  klfer_log(const char *, char);
static int  klfer_register_func(struct klfer_func_cfg *);
static int  klfer_unregister_func(struct klfer_func_cfg *);
static void klfer_reset_funcs(void);
static int  klfer_set_params(int);
static void klfer_dump_settings(void);
static void klfer_print_log(int);
static void klfer_dump_logs(int, int);
static int  klfer_init_mod_data(void);
static void klfer_teardown_mod_data(void);
static int  klfer_open(struct inode *, struct file *);
static int  klfer_close(struct inode *, struct file *);
static long klfer_ioctl(struct file *, unsigned int, unsigned long);
static int  klfer_create_dev(void);
static void klfer_delete_dev(void);

/**
 * Module parameter
 */
static int MLOGS = MAX_LOGS;
module_param(MLOGS, int, S_IRUGO);
MODULE_PARM_DESC(MLOGS, "Max number of logs to be saved.");

/**
 * Module data info
 */
struct klfer_mod_data modData;

/**
 * handler table
 */
struct file_operations klfer_fops = {
    .owner          = THIS_MODULE,
    .open           = klfer_open,
    .release        = klfer_close,
    .unlocked_ioctl = klfer_ioctl,
    .compat_ioctl   = klfer_ioctl, // for 32-bit App
};

/**
 * Unregister function registered by kretprobe
 * @param[in] func_idx Index of function
 */
static inline void klfer_unregister_kretprobe(int func_idx)
{
    unregister_kretprobe(&modData.funcs[func_idx].krp);
    modData.funcs[func_idx].b_registered = false;
    pr_info("Unregister return probe at %s: %p\n",
            modData.funcs[func_idx].krp.kp.symbol_name,
            modData.funcs[func_idx].krp.kp.addr);
}

/**
 * Handler function to be called when the registered function is called
 * @param[in] *ri   kretprobe instance
 * @param[in] *regs Not used
 * @retval KLFER_OK  Success
 * @retval KLFER_Err Error
 */
static int  klfer_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    return (modData.b_logging ? klfer_log(ri->rp->kp.symbol_name, 'e') : KLFER_OK);
}

/**
 * Handler function to be called when the registered function returns
 * @param[in] *ri   kretprobe instance
 * @param[in] *regs Not used
 * @retval KLFER_OK  Success
 * @retval KLFER_Err Error
 */
static int klfer_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    return (modData.b_logging ? klfer_log(ri->rp->kp.symbol_name, 'r') : KLFER_OK);
}

/**
 * Logger function
 * @param[in] *func_name Called function name
 * @param[in] event_id   Event ID ('e': Entry / 'r': Return)
 * @retval KLFER_OK  Success
 * @retval KLFER_Err Error
 */
static int klfer_log(const char *func_name, char event_id)
{
    int func_idx;

    if(modData.num_of_logs >= MAX_LOGS)
    {
        pr_err("Err: No log space - %s\n", func_name);
        return KLFER_ERR;
    }
    for(func_idx=0; func_idx<modData.num_of_funcs; func_idx++)
    {
        if(strcmp(func_name, modData.funcs[func_idx].func_name) == 0)
        {
            if(!modData.funcs[func_idx].b_registered)
            {
                pr_err("Err: Not registered - %s\n", func_name);
                return KLFER_ERR;
            }
        }
        if(modData.funcs[func_idx].b_registered &&
           strcmp(func_name, modData.funcs[func_idx].func_name) == 0)
        {
            break;
        }
    }
    if(func_idx >= modData.num_of_funcs)
    {
        pr_err("Err: No entry - %s\n", func_name);
        return KLFER_ERR;
    }

    if(modData.b_timestamp)
    {
        getnstimeofday(&modData.logs[modData.num_of_logs].time);
    }
    modData.logs[modData.num_of_logs].func_idx = func_idx;
    modData.logs[modData.num_of_logs].event_id = event_id;

    if(modData.b_jit_log)
    {
        klfer_print_log(modData.num_of_logs);
    }

    modData.num_of_logs++;
    return KLFER_OK;
}

/**
 * Dump logs
 * @param[in] start_idx Start index of the log to print
 * @param[in] num       Number of logs to print
 */
static void klfer_dump_logs(int start_idx, int num)
{
    int i;

    if(start_idx >= modData.num_of_logs) return;
    if((start_idx + num) > modData.num_of_logs) num = (modData.num_of_logs - start_idx);
    for(i=start_idx; i<start_idx+num; i++)
    {
        klfer_print_log(i);
    }
}

/**
 * Register function
 * @param[in] *cfg Configurations for registration
 * @retval  KLFER_OK Success
 * @retval -EALREADY Same function is already registered
 * @retval -ENOBUFS  Maximum number of registrations has been reached
 */
static int klfer_register_func(struct klfer_func_cfg *cfg)
{
    int func_idx, ret;

    /* search same function */
    for(func_idx=0; func_idx<modData.num_of_funcs; func_idx++)
    {
        if(strcmp(modData.funcs[func_idx].func_name, cfg->func_name) == 0)
        {
            if(modData.funcs[func_idx].b_registered == true)
            {
                pr_err("%s is already registered.\n", cfg->func_name);
                return -EALREADY;
            }
        }
    }
    if(func_idx == MAX_REG_FUNCS)
    {
        pr_err("Too many funcs registered.\n");
        return -ENOBUFS;
    }
    if(func_idx == modData.num_of_funcs)
    {
        /* new function */
        strcpy(modData.funcs[func_idx].func_name, cfg->func_name);
        modData.num_of_funcs++;
    }
    modData.funcs[func_idx].b_registered = true;
    /* register kretprobe */
    modData.funcs[func_idx].krp.kp.symbol_name = modData.funcs[func_idx].func_name;
    modData.funcs[func_idx].krp.handler = klfer_ret_handler;
    modData.funcs[func_idx].krp.entry_handler = klfer_entry_handler;
    modData.funcs[func_idx].krp.maxactive = 20;
    ret = register_kretprobe(&modData.funcs[func_idx].krp);
    if(ret < 0)
    {
        pr_err("register_kretprobe() failed. > %s() (returned: %d)\n", cfg->func_name, ret);
        klfer_unregister_func(cfg);
        return ret;
    }
    pr_info("Register return probe at %s: %p\n",
            modData.funcs[func_idx].krp.kp.symbol_name,
            modData.funcs[func_idx].krp.kp.addr);
    return KLFER_OK;
}

/**
 * Unregister function
 * @param[in] *cfg Configurations for registration
 * @retval  KLFER_OK Success
 * @retval -ESRCH    The function is not registered yet
 */
static int klfer_unregister_func(struct klfer_func_cfg *cfg)
{
    int func_idx;

    for(func_idx=0; func_idx<modData.num_of_funcs; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered &&
           strcmp(cfg->func_name, modData.funcs[func_idx].func_name) == 0)
        {
            klfer_unregister_kretprobe(func_idx);
            return KLFER_OK;
        }
    }
    pr_err("Err: %s() is not registered.\n", cfg->func_name);
    return -ESRCH;
}

/**
 * Unregister all registered functions
 */
static void klfer_reset_funcs(void)
{
    int func_idx;

    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered)
        {
            klfer_unregister_kretprobe(func_idx);
        }
    }
    modData.num_of_funcs = 0;
}

/**
 * Set control parameters
 * @param[in] ctrl_param Control parameters
 * @retval KLFER_OK  Success
 * @retval KLFER_ERR Error
 */
static int klfer_set_params(int ctrl_param)
{
    /* Logger control */
    if(ctrl_param & (UPDATE_FLAG << LOGGER_CTRL_SHIFT))
    {
        if(ctrl_param & (VALUE_BIT << LOGGER_CTRL_SHIFT))
            modData.b_logging = true;
        else
            modData.b_logging = false;
    }
    /* JIT print log control */
    if(ctrl_param & (UPDATE_FLAG << JIT_CTRL_SHIFT))
    {
        if(ctrl_param & (VALUE_BIT << JIT_CTRL_SHIFT))
            modData.b_jit_log = true;
        else
            modData.b_jit_log = false;
    }
    /* Timestamp control */
    if(ctrl_param & (UPDATE_FLAG << TIMESTAMP_CTRL_SHIFT))
    {
        /* Enable / Disable */
        if(ctrl_param & (VALUE_BIT << TIMESTAMP_CTRL_SHIFT))
            modData.b_timestamp = true;
        else
            modData.b_timestamp = false;

        /* Format */
        modData.timestamp_fmt = TS_FMT_MASK(ctrl_param);
    }
    return KLFER_OK;
}

/**
 * Dump current settings and registered functions
 */
static void klfer_dump_settings(void)
{
    int func_idx;
    char ts_fmt[40];

    switch(modData.timestamp_fmt)
    {
    case TS_FMT_ABS:
        strcpy(ts_fmt, "Absolute time");
        break;
    case TS_FMT_RLTV_FIRST:
        strcpy(ts_fmt, "Relative time from the first log");
        break;
    case TS_FMT_RLTV_PREV:
        strcpy(ts_fmt, "Relative time from the previous log");
        break;
    default:
        strcpy(ts_fmt, "Unknown format");
        break;
    }

    /* Dump parameter settings */
    printk("Logger        : %s\n", (modData.b_logging ?   "Enable" : "Disable"));
    printk("JIT print log : %s\n", (modData.b_jit_log ?   "Enable" : "Disable"));
    printk("Timestamp     : %s\n", (modData.b_timestamp ? "Enable" : "Disable"));
    printk("Timestamp fmt : %s\n", ts_fmt);

    /* Dump registered functions */
    printk("[Indx] [Reg] function_name\n");
    for(func_idx=0; func_idx<modData.num_of_funcs; func_idx++)
    {
        printk("[%4d] [ %c ] %s\n", func_idx, (modData.funcs[func_idx].b_registered ? 'Y' : 'N'),
                modData.funcs[func_idx].func_name);
    }
}

/**
 * Print event log
 * @param[in] log_idx Log index (0 origin)
 */
static void klfer_print_log(int log_idx)
{
    char buf[MAX_STR_LEN * 3];
    int offset = 0;
    long timestamp;
    struct klfer_log *log = &modData.logs[log_idx];
    struct klfer_log *rltv_log = NULL;

    if(modData.b_timestamp)
    {
        timestamp = log->time.tv_sec * NSEC_PER_SEC + log->time.tv_nsec;
        switch(modData.timestamp_fmt)
        {
        case TS_FMT_RLTV_FIRST:
            rltv_log = &modData.logs[0];
            break;
        case TS_FMT_RLTV_PREV:
            if(log_idx > 0) rltv_log = &modData.logs[log_idx - 1];
            else rltv_log = &modData.logs[0];
            break;
        default:
            break;
        }
        if(rltv_log)
            timestamp -= rltv_log->time.tv_sec * NSEC_PER_SEC + rltv_log->time.tv_nsec;
        offset = snprintf(buf, 32, "[ %20ld nsec] ", timestamp);
    }
    snprintf(buf + offset, MAX_STR_LEN * 3 - offset, "[%d] %c %s",
             log_idx + 1,
             log->event_id,
             modData.funcs[log->func_idx].func_name);
    printk("%s\n", buf);
}

/**
 * Initialize module data
 * @retval KLFER_OK Success
 * @retval -ENOBUFS Failed to kmalloc
 */
static int klfer_init_mod_data(void)
{
    int i;
    modData.pclass = NULL;
    modData.pdev = NULL;
    atomic_set(&modData.open_available, 1);
    modData.num_of_funcs = 0;
    modData.num_of_logs = 0;
    modData.b_logging = false;
    modData.b_jit_log = false;
    modData.b_timestamp = true;
    modData.timestamp_fmt = TS_FMT_ABS;
    for(i=0; i<MAX_REG_FUNCS; i++)
    {
        modData.funcs[i].b_registered = false;
    }
    modData.logs = (struct klfer_log *)kmalloc(sizeof(struct klfer_log) * MLOGS, GFP_KERNEL);
    if(!modData.logs)
    {
        return -ENOBUFS;
    }

    return KLFER_OK;
}

/**
 * Teardown module data
 */
static void klfer_teardown_mod_data(void)
{
    klfer_reset_funcs();
    if(modData.logs)
    {
        kfree(modData.logs);
    }
}

/**
 * Open klfer device file
 * @param[in] *ind  Not use
 * @param[in] *filp Not use
 * @retval KLFER_OK Success
 * @retval -EBUSY   Device file is open already
 */
static int klfer_open(struct inode *ind, struct file *filp)
{
    if(!atomic_dec_and_test(&modData.open_available))
    {
        atomic_inc(&modData.open_available);
        pr_err("klfer is open already.\n");
        return -EBUSY;
    }
    pr_debug("Open klfer.\n");
    return KLFER_OK;
}

/**
 * Close klfer device file
 * @param[in] *ind  Not use
 * @param[in] *filp Not use
 * @retval KLFER_OK Success
 */
static int klfer_close(struct inode *ind, struct file *filp)
{
    atomic_inc(&modData.open_available);
    pr_debug("Close klfer.\n");
    return KLFER_OK;
}

/**
 * Handler for ioctl
 * @param[in] *filp Not use
 * @param[in] cmd   Request command
 * @param[in] arg   Argument pointer in user space
 * @retval KLFER_OK Success
 * @retval -EINVAL  Request command is not supported
 * @retval -EFAULT  arg is pointed unacceptable space
 */
static long klfer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct klfer_func_cfg func_cfg;
    int ctrl_param;
    int ret = KLFER_OK;
    int err;

    pr_debug("ioctl command: %d\n", _IOC_NR(cmd));
    switch(_IOC_NR(cmd))
    {
    case KLFER_REG_FUNC_FLAG:
        err = copy_from_user(&func_cfg, (void *)arg, sizeof(func_cfg));
        if(err) goto ERR_COPY_FROM_USER;
        if(func_cfg.b_reg)
            ret = klfer_register_func(&func_cfg);
        else
            ret = klfer_unregister_func(&func_cfg);
        break;
    case KLFER_RESET_FLAG:
        klfer_reset_funcs();
        modData.num_of_logs = 0;
        break;
    case KLFER_SET_PARAMS_FLAG:
        err = copy_from_user(&ctrl_param, (void *)arg, sizeof(ctrl_param));
        if(err) goto ERR_COPY_FROM_USER;
        ret = klfer_set_params(ctrl_param);
        break;
    case KLFER_DUMP_SETTINGS_FLAG:
        klfer_dump_settings();
        break;
    case KLFER_DUMP_LOGS_FLAG:
        klfer_dump_logs(0, modData.num_of_logs);
        break;
#ifdef DEBUG
    case KLFER_SAMPLE_FLAG:
        klfer_sample_func();
        break;
#endif
    default:
        ret = -EINVAL;
    }
    return ret;
ERR_COPY_FROM_USER:
    return -EFAULT;
}

/**
 * Create character device file for klfer
 * @retval KLFER_OK  Success
 * @retval KLFER_ERR Error
 */
static int klfer_create_dev(void)
{
    struct device *dev;
    dev_t dev_no;

    /* Alloc major number */
    if(alloc_chrdev_region(&dev_no, MINOR_BASE, MINOR_NUM, KLFER_MOD_NAME))
    {
        pr_err("alloc_chrdev_region() error.\n");
        goto ALLOC_CHRDEV_REGION_ERR;
    }
    modData.major_num = MAJOR(dev_no);

    modData.pclass = class_create(THIS_MODULE, KLFER_MOD_NAME);
    if(IS_ERR(modData.pclass))
    {
        pr_err("class_create() error.\n");
        goto CLASS_CREATE_ERR;
    }
    cdev_init(&modData.chrdev, &klfer_fops);
    if(cdev_add(&modData.chrdev, dev_no, MINOR_NUM))
    {
        pr_err("cdev_add() error.\n");
        goto CDEV_ADD_ERR;
    }
    dev = device_create(modData.pclass, NULL, dev_no, NULL, KLFER_DEVICE_NAME);
    if(IS_ERR(dev))
    {
        pr_err("device_create() error.\n");
        goto DEVICE_CREATE_ERR;
    }
    modData.pdev = dev;

    return KLFER_OK;
DEVICE_CREATE_ERR:
    cdev_del(&modData.chrdev);
CDEV_ADD_ERR:
    class_destroy(modData.pclass);
CLASS_CREATE_ERR:
    unregister_chrdev_region(dev_no, MINOR_NUM);
ALLOC_CHRDEV_REGION_ERR:
    return KLFER_ERR;
}

/**
 * Delete character device file for klfer
 */
static void klfer_delete_dev(void)
{
    dev_t dev_no = MKDEV(modData.major_num, MINOR_BASE);

    device_destroy(modData.pclass, dev_no);
    cdev_del(&modData.chrdev);
    class_destroy(modData.pclass);
    unregister_chrdev_region(dev_no, MINOR_NUM);
}

/**
 * Handler when insmod
 * @retval KLFER_OK  Success
 * @retval KLFER_ERR Error
 */
static int __init klfer_init(void)
{
    if(klfer_init_mod_data())
    {
        return KLFER_ERR;
    }
    if(klfer_create_dev())
    {
        klfer_teardown_mod_data();
        return KLFER_ERR;
    }
    pr_info(KLFER_MOD_NAME "-" KLFER_MOD_VERSION ": loaded.\n");
    return KLFER_OK;
}

/**
 * Handler when rmmod
 */
static void __exit klfer_exit(void)
{
    klfer_teardown_mod_data();
    klfer_delete_dev();
    pr_info(KLFER_MOD_NAME "-" KLFER_MOD_VERSION ": unloaded.\n");
}

module_init(klfer_init);
module_exit(klfer_exit);

MODULE_DESCRIPTION("KLFER (Kernel Logger for Function Entries and Returns)");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(KLFER_MOD_VERSION);

