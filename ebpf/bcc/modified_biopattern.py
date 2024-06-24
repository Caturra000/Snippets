#!/usr/bin/env python
# @lint-avoid-python-3-compatibility-imports
#
# biopattern - Identify random/sequential and read/write disk access patterns.
#              For Linux, uses BCC, eBPF.
#
# Copyright (c) 2022 Rocky Xing.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# 21-Feb-2022   Rocky Xing   Created this.

from __future__ import print_function
from bcc import BPF
from time import sleep, strftime
import argparse
import os

examples = """examples:
    ./biopattern            # show block device I/O pattern.
    ./biopattern 1 10       # print 1 second summaries, 10 times
    ./biopattern -d sdb     # show sdb only
"""
parser = argparse.ArgumentParser(
    description="Show block device I/O pattern.",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-d", "--disk", type=str,
    help="Trace this disk only")
parser.add_argument("interval", nargs="?", default=99999999,
    help="Output interval in seconds")
parser.add_argument("count", nargs="?", default=99999999,
    help="Number of outputs")
args = parser.parse_args()
countdown = int(args.count)

bpf_text="""
#include <trace/events/block.h>

enum OP {
    OP_READ = 0,
    OP_WRITE,
    OP_DISCARD,
    OP_FLUSH,

    OP_MAX
};

struct counter {
    u64 last_sector;
    u64 bytes;
    u32 sequential;
    u32 random;
    u32 ops[OP_MAX];
};

BPF_HASH(counters, u32, struct counter);

static int rwbs_unpack(const char *rwbs) {
    for (int i = 0; i < RWBS_LEN; i++) {
        switch (rwbs[i]) {
            case 'R': return OP_READ;
            case 'W': return OP_WRITE;
            case 'D': return OP_DISCARD;
            case 'F': return OP_FLUSH;
            case '\\0': return OP_MAX;
            default: (void)0;
        }
    }
    return OP_MAX;
}

TRACEPOINT_PROBE(block, block_rq_complete)
{
    struct counter *counterp;
    struct counter zero = {};
    u32 dev = args->dev;
    u64 sector = args->sector;
    u32 nr_sector = args->nr_sector;
    char *rwbs = args->rwbs;

    DISK_FILTER

    counterp = counters.lookup_or_try_init(&dev, &zero);
    if (counterp == 0) {
        return 0;
    }

    int op = rwbs_unpack(rwbs);

    if (counterp->last_sector) {
        if (counterp->last_sector == sector) {
            __sync_fetch_and_add(&counterp->sequential, 1);
        } else {
            __sync_fetch_and_add(&counterp->random, 1);
        }
        if (op != OP_MAX) {
            __sync_fetch_and_add(&counterp->ops[op], 1);
        }
        __sync_fetch_and_add(&counterp->bytes, nr_sector * 512);
    }
    counterp->last_sector = sector + nr_sector;

    return 0;
}
"""

dev_minor_bits = 20

def mkdev(major, minor):
   return (major << dev_minor_bits) | minor


partitions = {}

with open("/proc/partitions", 'r') as f:
    lines = f.readlines()
    for line in lines[2:]:
        words = line.strip().split()
        major = int(words[0])
        minor = int(words[1])
        part_name = words[3]
        partitions[mkdev(major, minor)] = part_name

if args.disk is not None:
    disk_path = os.path.join('/dev', args.disk)
    if os.path.exists(disk_path) == False:
        print("no such disk '%s'" % args.disk)
        exit(1)

    stat_info = os.stat(disk_path)
    major = os.major(stat_info.st_rdev)
    minor = os.minor(stat_info.st_rdev)
    bpf_text = bpf_text.replace('DISK_FILTER',
                                'if (dev != %s) { return 0; }' % mkdev(major, minor))
else:
    bpf_text = bpf_text.replace('DISK_FILTER', '')

b = BPF(text=bpf_text)

# check whether hash table batch ops is supported
htab_batch_ops = True if BPF.kernel_struct_has_field(b'bpf_map_ops',
        b'map_lookup_and_delete_batch') == 1 else False

exiting = 0 if args.interval else 1
counters = b.get_table("counters")

print("%-9s %-7s %5s %5s %6s %7s %9s %7s %8s %10s" % 
    ("TIME", "DISK", "%RND", "%SEQ", "%READ", "%WRITE", "%DISCARD", "%FLUSH", "COUNT", "KBYTES"))

while True:
    try:
        sleep(int(args.interval))
    except KeyboardInterrupt:
        exiting = 1
    
    for k, v in (counters.items_lookup_and_delete_batch()
                if htab_batch_ops else counters.items()):
        total = v.random + v.sequential
        if total == 0:
            continue

        part_name = partitions.get(k.value, "Unknown")
        random_percent = int(round(v.random * 100 / total))
        sequential_percent = 100 - random_percent
        read_percent = int(round(v.ops[0] * 100 / total))
        write_percent = int(round(v.ops[1] * 100 / total))
        discard_percent = int(round(v.ops[2] * 100 / total))
        flush_percent = 100 - read_percent - write_percent - discard_percent

        print("%-9s %-7s %5s %5s %6s %7s %9s %7s %8d %10d" % (
            strftime("%H:%M:%S"),
            part_name,
            random_percent,
            sequential_percent,
            read_percent,
            write_percent,
            discard_percent,
            flush_percent,
            total,
            v.bytes / 1024))

    if not htab_batch_ops:
        counters.clear()

    countdown -= 1
    if exiting or countdown == 0:
        exit()
