/**
 * @file  klfer_dbg.c
 * @brief debug file for KLFER (Kernel Logger for Function Entries and Returns)
 */

#include "klfer.h"
#include "klfer_dbg.h"

int klfer_sample_func(void)
{
    int i;
    pr_debug("enter %s()\n", __func__);
    for(i=0; i<5; i++)
    {
        klfer_sample_nested_func();
    }
    return KLFER_OK;
}
EXPORT_SYMBOL(klfer_sample_func);

void klfer_sample_nested_func(void)
{
    pr_debug("enter %s()\n", __func__);
}
EXPORT_SYMBOL(klfer_sample_nested_func);

