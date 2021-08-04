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
static void klfer_print_reg_funcs(void);
static void klfer_init_mod_data(void);
static int  klfer_open(struct inode *, struct file *);
static int  klfer_close(struct inode *, struct file *);
static long klfer_ioctl(struct file *, unsigned int, unsigned long);
static int  klfer_create_dev(void);
static void klfer_delete_dev(void);

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
    modData.grps[modData.funcs[func_idx].grp_idx].num_of_funcs--;
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
    return klfer_log(ri->rp->kp.symbol_name, 'e');
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
    return klfer_log(ri->rp->kp.symbol_name, 'r');
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
    char buf[MAX_STR_LEN * 3];
    int offset = 0;
    int func_idx;
    struct timespec time;
    long timestamp;

    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered &&
           strcmp(func_name, modData.funcs[func_idx].func_name) == 0)
        {
            break;
        }
    }
    if(func_idx >= MAX_REG_FUNCS)
    {
        pr_err("Err: No entry - %s\n", func_name);
        return KLFER_ERR;
    }
    if(modData.funcs[func_idx].grp_idx >= MAX_GRPS)
    {
        pr_err("Err: No group - %s\n", func_name);
        return KLFER_ERR;
    }

    if(modData.b_logging)
    {
        if(modData.funcs[func_idx].b_rec_timestamp)
        {
            getnstimeofday(&time);
            timestamp = time.tv_sec * NSEC_PER_SEC + time.tv_nsec;
            offset = snprintf(buf, 32, "[ %ld nsec] ", timestamp);
        }
        snprintf(buf + offset, MAX_STR_LEN * 3 - offset, "%s[%d] %c %s",
                 modData.grps[modData.funcs[func_idx].grp_idx].grp_id,
                 modData.grps[modData.funcs[func_idx].grp_idx].cnt++,
                 event_id,
                 func_name);
        pr_info("%s\n", buf);
    }
    return KLFER_OK;
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
    int func_idx, grp_idx, ret;

    /* search same function */
    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered == true &&
           strcmp(modData.funcs[func_idx].func_name, cfg->func_name) == 0)
        {
            pr_err("%s is already registered.\n", cfg->func_name);
            return -EALREADY;
        }
    }
    /* search free function index */
    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered == false)
        {
            break;
        }
    }
    if(func_idx == MAX_REG_FUNCS)
    {
        pr_err("Too many funcs registered.\n");
        return -ENOBUFS;
    }
    strcpy(modData.funcs[func_idx].func_name, cfg->func_name);
    modData.funcs[func_idx].b_registered = true;
    modData.funcs[func_idx].b_rec_timestamp = cfg->b_rec_timestamp;
    /* search same group ID */
    for(grp_idx=0; grp_idx<MAX_GRPS; grp_idx++)
    {
        if(modData.grps[grp_idx].num_of_funcs > 0 &&
           strcmp(modData.grps[grp_idx].grp_id, cfg->grp_id) == 0)
        {
            /* found same group ID */
            modData.grps[grp_idx].num_of_funcs++;
            modData.funcs[func_idx].grp_idx = grp_idx;
            break;
        }
    }
    if(grp_idx == MAX_GRPS)
    {
        /* search free group index */
        for(grp_idx=0; grp_idx<MAX_GRPS; grp_idx++)
        {
            if(modData.grps[grp_idx].num_of_funcs == 0)
            {
                /* found free group index */
                strcpy(modData.grps[grp_idx].grp_id, cfg->grp_id);
                modData.grps[grp_idx].cnt = 0;
                modData.grps[grp_idx].num_of_funcs = 1;
                break;
            }
        }
    }
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

    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
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
}

/**
 * Print all registered functions
 */
static void klfer_print_reg_funcs(void)
{
    int func_idx;

    printk("[Func] function_name                    [Grp] [TS]\n");
    for(func_idx=0; func_idx<MAX_REG_FUNCS; func_idx++)
    {
        if(modData.funcs[func_idx].b_registered)
        {
            printk("[%4d] %-32s [%3u] [%2c]\n",
                    func_idx,
                    modData.funcs[func_idx].func_name,
                    modData.funcs[func_idx].grp_idx,
                    (modData.funcs[func_idx].b_rec_timestamp ? 'y' : 'n'));
        }
    }
}

/**
 * Initialize module data
 */
static void klfer_init_mod_data(void)
{
    int i;
    modData.pclass = NULL;
    modData.pdev = NULL;
    atomic_set(&modData.open_available, 1);
    modData.b_logging = false;
    for(i=0; i<MAX_GRPS; i++)
    {
        modData.grps[i].num_of_funcs = 0;
        modData.grps[i].cnt = 0;
    }
    for(i=0; i<MAX_REG_FUNCS; i++)
    {
        modData.funcs[i].b_registered = false;
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
 * @retval KLFER_OK  Success
 * @retval -EALREADY Already enable logger / disable logger
 * @retval -EBADR    Request command is not supported
 * @retval -EFAULT   arg is pointed unacceptable space
 */
static long klfer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct klfer_func_cfg func_cfg;
    int ret = KLFER_OK;
    int err;

    pr_debug("ioctl command: %d\n", _IOC_NR(cmd));
    switch(_IOC_NR(cmd))
    {
    case KLFER_REG_FUNC_FLAG:
        err = copy_from_user(&func_cfg, (void *)arg, sizeof(func_cfg));
        if(err) goto ERR_COPY_FROM_USER;
        ret = klfer_register_func(&func_cfg);
        break;
    case KLFER_UNREG_FUNC_FLAG:
        err = copy_from_user(&func_cfg, (void *)arg, sizeof(func_cfg));
        if(err) goto ERR_COPY_FROM_USER;
        ret = klfer_unregister_func(&func_cfg);
        break;
    case KLFER_RESET_FLAG:
        klfer_reset_funcs();
        break;
    case KLFER_ENABLE_LOG_FLAG:
        if(modData.b_logging)
        {
            ret = -EALREADY;
        }
        else
        {
            pr_debug("Enable logger\n");
            modData.b_logging = true;
        }
        break;
    case KLFER_DISABLE_LOG_FLAG:
        if(!modData.b_logging)
        {
            ret = -EALREADY;
        }
        else
        {
            modData.b_logging = false;
            pr_debug("Disable logger\n");
        }
        break;
    case KLFER_PRINT_REG_FUNCS_FLAG:
        klfer_print_reg_funcs();
        break;
#ifdef DEBUG
    case KLFER_SAMPLE_FLAG:
        klfer_sample_func();
        break;
#endif
    default:
        ret = -EBADR;
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
    klfer_init_mod_data();
    if(klfer_create_dev())
    {
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
    klfer_reset_funcs();
    klfer_delete_dev();
    pr_info(KLFER_MOD_NAME "-" KLFER_MOD_VERSION ": unloaded.\n");
}

module_init(klfer_init);
module_exit(klfer_exit);

MODULE_DESCRIPTION("KLFER (Kernel Logger for Function Entries and Returns)");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(KLFER_MOD_VERSION);

