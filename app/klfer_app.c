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
#define APP_VERSION "0.1"

#define DEVICE_FILE_PATH ("/dev/" KLFER_DEVICE_NAME)
#define DEFAULT_GRP_ID   "NO_GRP"

#define ARG_REQ(s) (strcmp(s, argv[1]) == 0)

/**
 * Usage
 */
static void usage(void)
{
    printf("Usage:\n");
    printf("  %s { -A <FUNC_NAME> [OPTIONS] | -D <FUNC_NAME> | -R | -e | -d | -p | -h }\n\n", APP);
    printf("    -A     Add new function to be registered\n");
    printf("    -D     Delete registered function\n");
    printf("    -R     Reset all registered functions\n");
    printf("    -e     Enable logger\n");
    printf("    -d     Disable logger\n");
    printf("    -p     Print registered functions\n");
    printf("    -h     Help\n\n");
    printf("  FUNC_NAME:\n");
    printf("    function name to be logged. MUST be symbol in kernel\n\n");
    printf("  OPTIONS:\n");
    printf("    -g <GROUP_NAME>  Group name to display in the log (default: \"%s\"\n", DEFAULT_GRP_ID);
    printf("    -t               Whether to show the timestamp in the log (default: no)\n\n");
#ifdef DEBUG
    printf("  TEST COMMAND:\n");
    printf("    -T     Call sample function (klfer_sample_func)\n");
    printf(" ex) # %s -T\n\n", APP);
#endif
}

/**
 * Command by ioctl
 * @param[in] cmd       Command ID
 * @param[in] *func_cfg Command configurations
 * @retval  0 Success
 * @retval -1 Error
 */
int klfer_command(int cmd, struct klfer_func_cfg *func_cfg)
{
    int fd;

    fd = open(DEVICE_FILE_PATH, O_RDWR);
    if(fd < 0)
    {
        perror("open");
        return -1;
    }
    if(ioctl(fd, cmd, func_cfg) < 0)
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
    char *options = "A:D:Redphg:tT";
#else
    char *options = "A:D:Redphg:t";
#endif
    struct klfer_func_cfg func_cfg =
    {
        .func_name = "",
        .grp_id = DEFAULT_GRP_ID,
        .b_rec_timestamp = false
    };

    if(argc < 2) goto ERR_ARG;

    opterr = 0; // disable error message of getopt()
    while((opt = getopt(argc, argv, options)) != -1)
    {
        switch(opt)
        {
        case 'A':
            cmd = KLFER_REG_FUNC;
            strcpy(func_cfg.func_name, optarg);
            break;
        case 'D':
            cmd = KLFER_UNREG_FUNC;
            strcpy(func_cfg.func_name, optarg);
            break;
        case 'R':
            cmd = KLFER_RESET;
            break;
        case 'e':
            cmd = KLFER_ENABLE_LOG;
            break;
        case 'd':
            cmd = KLFER_DISABLE_LOG;
            break;
        case 'p':
            cmd = KLFER_PRINT_REG_FUNCS;
            break;
        case 'h':
            usage();
            return 0;
        case 'g':
            strcpy(func_cfg.grp_id, optarg);
            break;
        case 't':
            func_cfg.b_rec_timestamp = true;
            break;
#ifdef DEBUG
        case 'T':
            cmd = KLFER_SAMPLE;
            break;
#endif
        default:
            goto ERR_ARG;
        }
    }
    if(cmd == KLFER_NO_COMMAND) goto ERR_ARG;

    return klfer_command(cmd, &func_cfg);
ERR_ARG:
    usage();
    return -1;
}

