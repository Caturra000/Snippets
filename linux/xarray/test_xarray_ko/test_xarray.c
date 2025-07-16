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

void xa_dump_node(const struct xa_node *node)
{
    unsigned i, j;

    if (!node)
        return;
    if ((unsigned long)node & 3) {
        pr_cont("node %px\n", node);
        return;
    }

    pr_cont("node %px %s %d parent %px shift %d count %d values %d "
        "array %px list %px %px marks",
        node, node->parent ? "offset" : "max", node->offset,
        node->parent, node->shift, node->count, node->nr_values,
        node->array, node->private_list.prev, node->private_list.next);
    for (i = 0; i < XA_MAX_MARKS; i++)
        for (j = 0; j < XA_MARK_LONGS; j++)
            pr_cont(" %lx", node->marks[i][j]);
    pr_cont("\n");
}

static void xa_dump_index(unsigned long index, unsigned int shift)
{
    if (!shift)
        pr_info("%lu: ", index);
    else if (shift >= BITS_PER_LONG)
        pr_info("0-%lu: ", ~0UL);
    else
        pr_info("%lu-%lu: ", index, index | ((1UL << shift) - 1));
}

static void xa_dump_entry(const void *entry, unsigned long index, unsigned long shift)
{
    if (!entry)
        return;

    xa_dump_index(index, shift);

    if (xa_is_node(entry)) {
        if (shift == 0) {
            pr_cont("%px\n", entry);
        } else {
            unsigned long i;
            struct xa_node *node = xa_to_node(entry);
            xa_dump_node(node);
            for (i = 0; i < XA_CHUNK_SIZE; i++)
                xa_dump_entry(node->slots[i],
                      index + (i << node->shift), node->shift);
        }
    } else if (xa_is_value(entry))
        pr_cont("value %ld (0x%lx) [%px]\n", xa_to_value(entry),
                        xa_to_value(entry), entry);
    else if (!xa_is_internal(entry))
        pr_cont("%px\n", entry);
    else if (xa_is_retry(entry))
        pr_cont("retry (%ld)\n", xa_to_internal(entry));
    else if (xa_is_sibling(entry))
        pr_cont("sibling (slot %ld)\n", xa_to_sibling(entry));
    else if (xa_is_zero(entry))
        pr_cont("zero (%ld)\n", xa_to_internal(entry));
    else
        pr_cont("UNKNOWN ENTRY (%px)\n", entry);
}

void xa_dump(const struct xarray *xa)
{
    void *entry = xa->xa_head;
    unsigned int shift = 0;

    pr_info("xarray: %px head %px flags %x marks %d %d %d\n", xa, entry,
            xa->xa_flags, xa_marked(xa, XA_MARK_0),
            xa_marked(xa, XA_MARK_1), xa_marked(xa, XA_MARK_2));
    if (xa_is_node(entry))
        shift = xa_to_node(entry)->shift + XA_CHUNK_SHIFT;
    xa_dump_entry(entry, 0, shift);
}

// lx-symbols在init时不方便，所以放到exit
// 要是只dump不debug的话，不需要特意放到exit
/*
测试结果：
rmmod test_xarray.ko
[  122.538316] xarray: ffffc90000633d38 head 00000000000007cd flags 0 marks 0 0 0
[  122.538763] 0: value 998 (0x3e6) [00000000000007cd]
[  122.539058] xarray: ffffc90000633d38 head ffff88800093548a flags 0 marks 0 0 0
[  122.539421] 0-63: node ffff888000935488 max 3 parent 0000000000000000 shift 0 count 2 values 2 array ffffc90000633d30
[  122.540128] 0: value 998 (0x3e6) [00000000000007cd]
[  122.540381] 61: value 244 (0xf4) [00000000000001e9]
[  122.540651] xarray: ffffc90000633d38 head ffff888000935b62 flags 0 marks 0 0 0
[  122.541160] 0-4095: node ffff888000935b60 max 15 parent 0000000000000000 shift 6 count 2 values 0 array ffffc90000630
[  122.541780] 0-63: node ffff888000935488 offset 0 parent ffff888000935b60 shift 0 count 2 values 2 array ffffc90000630
[  122.542407] 0: value 998 (0x3e6) [00000000000007cd]
[  122.542755] 61: value 244 (0xf4) [00000000000001e9]
[  122.543209] 64-127: node ffff888000935918 offset 1 parent ffff888000935b60 shift 0 count 1 values 1 array ffffc900000
[  122.543752] 127: value 353 (0x161) [00000000000002c3]
[  122.544240] xarray test module unloaded
*/
static void __exit xarray_test_exit(void)
{
    struct xarray xa_stack;

    struct xarray *xa = &xa_stack;
    xa_init(xa);

    xa_store(xa, 0, xa_mk_value(998), GFP_KERNEL);
    xa_dump(xa);

    xa_store(xa, 61, xa_mk_value(244), GFP_KERNEL);
    xa_dump(xa);

    xa_store(xa, 127, xa_mk_value(353), GFP_KERNEL);
    xa_dump(xa);

    xa_destroy(xa);
    printk(KERN_INFO "xarray test module unloaded\n");
}

module_init(xarray_test_init);
module_exit(xarray_test_exit);
