/**
 * @file  klfer_app.c
 * @brief KLFER (Kernel Logger for Function Entries and Returns) application
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "klfer_api.h"

#define APP "klferctl"
#define APP_VERSION "0.4"

#define DEVICE_FILE_PATH ("/dev/" KLFER_DEVICE_NAME)
#define ARG_REQ(s) (strcmp(s, argv[1]) == 0)
#define KLFER_NO_COMMAND -1

/**
 * Usage
 */
static void usage(void)
{
    printf("Usage:\n");
    printf("  %s {-A <FUNC>|-D <FUNC>|-R|{[-E|-d] [-J|-j] [-T<FMT>|-t]}|-S|-L|-h}\n\n", APP);
    printf("    -A <FUNC>     Add new function(<FUNC>(*1)) to be registered\n");
    printf("    -D <FUNC>     Delete registered function(<FUNC>(*1))\n");
    printf("    -R            Reset (delete all registered functions and logs)\n");
    printf("    -E | -d       Enable logger(-E) / Disable logger(-d) (default: Disable)\n");
    printf("    -J | -j       Enable JIT print log(*3)(-J) / Disable JIT print log(-j) (default: Disable)\n");
    printf("    -T<FMT> | -t  Enable Timestamp(-T<FMT>(*2)) / Disable Timestamp(-t) (default: Enable)\n");
    printf("    -S            Dump current settings and registered functions\n");
    printf("    -L            Dump Logs\n");
    printf("    -h            Help\n\n");
#ifdef DEBUG
    printf("  SAMPLE COMMAND:\n");
    printf("    -s            Call sample function (klfer_sample_func)\n");
    printf("    ex) $ %s -s\n\n", APP);
#endif
    printf("  (*1) <FUNC>       : Function name to be logged. MUST be symbol in kernel\n");
    printf("  (*2) <FMT> {0..2} : Timestamp output format\n");
    printf("     # -T0 > Absolute time (default)\n");
    printf("     # -T1 > Relative time from the first log\n");
    printf("     # -T2 > Relative time from the previous log\n");
    printf("  (*3) JIT(Just-In-Time) print log\n");
    printf("     # Print a log each time.\n");
    printf("     # So, timestamp contains printk processing time.\n");
    printf("     # Just-In-Time print log should not be used \n");
    printf("     #   if you want to measure function processing time.\n");
}

/**
 * Command by ioctl
 * @param[in] cmd      Command ID
 * @param[in] *param   Command configurations
 * @retval  0 Success
 * @retval -1 Error
 */
int klfer_command(int cmd, void *param)
{
    int fd;

    fd = open(DEVICE_FILE_PATH, O_RDWR);
    if(fd < 0)
    {
        perror("open");
        return -1;
    }
    if(ioctl(fd, cmd, param) < 0)
    {
        perror("ioctl");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/**
 * Main function
 * @param[in] argc    Number of arguments
 * @param[in] *argv[] Arguments
 * @retval  0 Success
 * @retval -1 Error
 */
int main(int argc, char *argv[])
{
    int opt;
    int cmd = KLFER_NO_COMMAND;
#ifdef DEBUG
    char *options = "A:D:REdJjT:tSLhs";
#else
    char *options = "A:D:REdJjT:tSLh";
#endif
    struct klfer_func_cfg func_cfg =
    {
        .func_name = "",
        .b_reg = false
    };
    int ctrl_param = 0;
    void *param = NULL;

    if(argc < 2) goto ERR_ARG;

    opterr = 0; // disable error message of getopt()
    while((opt = getopt(argc, argv, options)) != -1)
    {
        switch(opt)
        {
        case 'A':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_REG_FUNC;
            strcpy(func_cfg.func_name, optarg);
            func_cfg.b_reg = true;
            param = &func_cfg;
            break;
        case 'D':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_REG_FUNC;
            strcpy(func_cfg.func_name, optarg);
            func_cfg.b_reg = false;
            param = &func_cfg;
            break;
        case 'R':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_RESET;
            break;
        case 'E':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            ENABLE_LOGGER(ctrl_param);
            break;
        case 'd':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            DISABLE_LOGGER(ctrl_param);
            break;
        case 'J':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            ENABLE_JIT(ctrl_param);
            break;
        case 'j':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            DISABLE_JIT(ctrl_param);
            break;
        case 'T':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            ENABLE_TS(ctrl_param);
            switch(atoi(optarg))
            {
            case TS_FMT_ABS:
                SET_TS_FMT_ABS(ctrl_param);
                break;
            case TS_FMT_RLTV_FIRST:
                SET_TS_FMT_RLTV_FIRST(ctrl_param);
                break;
            case TS_FMT_RLTV_PREV:
                SET_TS_FMT_RLTV_PREV(ctrl_param);
                break;
            default:
                goto ERR_ARG;
            }
            break;
        case 't':
            if(cmd != KLFER_NO_COMMAND && cmd != KLFER_SET_PARAMS) goto ERR_ARG;
            cmd = KLFER_SET_PARAMS;
            param = &ctrl_param;
            DISABLE_TS(ctrl_param);
            break;
        case 'S':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_DUMP_SETTINGS;
            break;
        case 'L':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_DUMP_LOGS;
            break;
        case 'h':
            usage();
            return 0;
#ifdef DEBUG
        case 's':
            if(cmd != KLFER_NO_COMMAND) goto ERR_ARG;
            cmd = KLFER_SAMPLE;
            break;
#endif
        default:
            goto ERR_ARG;
        }
    }
    if(cmd == KLFER_NO_COMMAND) goto ERR_ARG;

    return klfer_command(cmd, param);
ERR_ARG:
    usage();
    return -1;
}

