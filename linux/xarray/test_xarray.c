#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/xarray.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caturra");
MODULE_DESCRIPTION("Test xarray store behavior");

static int __init xarray_test_init(void)
{
    printk(KERN_INFO "xarray test module init\n");
    return 0;
}

// lx-symbols在init时不方便，所以放到exit
static void __exit xarray_test_exit(void)
{
    struct xarray test_xarray;
    void *entry;
    unsigned long value;
    void *ret;

    xa_init(&test_xarray);

    printk(KERN_INFO "Storing value 9 at index 5\n");
    ret = xa_store(&test_xarray, 5, xa_mk_value(9), GFP_KERNEL);
    
    if (ret) {
        // TODO: xa err
        printk(KERN_ERR "xa_store failed with error %p\n", ret);
        goto out;
    }

    printk(KERN_INFO "Reading index 5\n");
    entry = xa_load(&test_xarray, 5);
    
    if (!entry) {
        printk(KERN_ERR "No entry found at index 5\n");
        goto out;
    }

    if (xa_is_value(entry)) {
        value = xa_to_value(entry);
        printk(KERN_INFO "Success! Retrieved value: %lu\n", value);
    } else {
        printk(KERN_ERR "Entry is not a value\n");
        goto out;
    }
out:
    xa_destroy(&test_xarray);
    printk(KERN_INFO "xarray test module unloaded\n");
}

module_init(xarray_test_init);
module_exit(xarray_test_exit);
