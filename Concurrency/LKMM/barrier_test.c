#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/barrier.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caturra");
MODULE_DESCRIPTION("Barriers");

#define $ asm volatile("pause" ::: "memory");


static int __init test_barriers(void)
{
    $
    smp_mb();
    $
    smp_rmb();
    $
    smp_wmb();
    $
    mb();
    $
    rmb();
    $
    wmb();
    return 0;
}

module_init(test_barriers);
