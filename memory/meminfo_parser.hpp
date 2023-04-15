#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <iostream>

// meminfo_parser
namespace mp {



/// interfaces



constexpr static bool CONFIG_DEBUG_MODE = false;

union Meminfo;
using Meminfo_field_type = long long;

// if successful, return 0
bool meminfo_parse(union Meminfo &meminfo);

void dump(Meminfo &meminfo, std::ostream &os = std::cout);

void dump_diff(Meminfo &before, Meminfo &after, Meminfo_field_type diff_kb, std::ostream &os = std::cout);



/// internals



template <typename ...Ts>
inline auto log(Ts &&...args) {
    if constexpr (CONFIG_DEBUG_MODE) {
        (std::cerr << ... << args) << std::endl;
    }
}

// $ cat /proc/meminfo | awk '{print "\""substr($1, 0, length($1)-1)"\","}'
const char *meminfo_names[] = {
    "MemTotal",
    "MemFree",
    "MemAvailable",
    "Buffers",
    "Cached",
    "SwapCached",
    "Active",
    "Inactive",
    "Active(anon)",
    "Inactive(anon)",
    "Active(file)",
    "Inactive(file)",
    "Unevictable",
    "Mlocked",
    "SwapTotal",
    "SwapFree",
    "Dirty",
    "Writeback",
    "AnonPages",
    "Mapped",
    "Shmem",
    "KReclaimable",
    "Slab",
    "SReclaimable",
    "SUnreclaim",
    "KernelStack",
    "PageTables",
    "NFS_Unstable",
    "Bounce",
    "WritebackTmp",
    "CommitLimit",
    "Committed_AS",
    "VmallocTotal",
    "VmallocUsed",
    "VmallocChunk",
    "Percpu",
    "AnonHugePages",
    "ShmemHugePages",
    "ShmemPmdMapped",
    "FileHugePages",
    "FilePmdMapped",
    "HugePages_Total",
    "HugePages_Free",
    "HugePages_Rsvd",
    "HugePages_Surp",
    "Hugepagesize",
    "Hugetlb",
    "DirectMap4k",
    "DirectMap2M",
    "DirectMap1G"
};

constexpr static const char MEMINFO_PATH[] = "/proc/meminfo";
constexpr static const size_t MEMINFO_TYPES = sizeof(meminfo_names) / sizeof(meminfo_names[0]);

union Meminfo {
    struct {
        Meminfo_field_type MemTotal;
        Meminfo_field_type MemFree;
        Meminfo_field_type MemAvailable;
        Meminfo_field_type Buffers;
        Meminfo_field_type Cached;
        Meminfo_field_type SwapCached;
        Meminfo_field_type Active;
        Meminfo_field_type Inactive;
        Meminfo_field_type Active_anon;
        Meminfo_field_type Inactive_anon;
        Meminfo_field_type Active_file;
        Meminfo_field_type Inactive_file;
        Meminfo_field_type Unevictable;
        Meminfo_field_type Mlocked;
        Meminfo_field_type SwapTotal;
        Meminfo_field_type SwapFree;
        Meminfo_field_type Dirty;
        Meminfo_field_type Writeback;
        Meminfo_field_type AnonPages;
        Meminfo_field_type Mapped;
        Meminfo_field_type Shmem;
        Meminfo_field_type KReclaimable;
        Meminfo_field_type Slab;
        Meminfo_field_type SReclaimable;
        Meminfo_field_type SUnreclaim;
        Meminfo_field_type KernelStack;
        Meminfo_field_type PageTables;
        Meminfo_field_type NFS_Unstable;
        Meminfo_field_type Bounce;
        Meminfo_field_type WritebackTmp;
        Meminfo_field_type CommitLimit;
        Meminfo_field_type Committed_AS;
        Meminfo_field_type VmallocTotal;
        Meminfo_field_type VmallocUsed;
        Meminfo_field_type VmallocChunk;
        Meminfo_field_type Percpu;
        Meminfo_field_type AnonHugePages;
        Meminfo_field_type ShmemHugePages;
        Meminfo_field_type ShmemPmdMapped;
        Meminfo_field_type FileHugePages;
        Meminfo_field_type FilePmdMapped;
        Meminfo_field_type HugePages_Total;
        Meminfo_field_type HugePages_Free;
        Meminfo_field_type HugePages_Rsvd;
        Meminfo_field_type HugePages_Surp;
        Meminfo_field_type Hugepagesize;
        Meminfo_field_type Hugetlb;
        Meminfo_field_type DirectMap4k;
        Meminfo_field_type DirectMap2M;
        Meminfo_field_type DirectMap1G;
    } field;

    Meminfo_field_type arr[MEMINFO_TYPES];
};

const char* meminfo_parse_line(Meminfo &meminfo, const char *cursor, size_t arr_index);

inline bool meminfo_parse(union Meminfo &meminfo) {
    auto do_syscall = [](auto syscall, auto &&...args) {
        int ret;
        while((ret = syscall(args...)) < 0 && errno == EINTR);
        return ret;
    };

    /// buffer

    constexpr size_t buf_size = 1<<16;
    char buf[buf_size];

    /// fd

    struct Fd_object {
        int fd;
        ~Fd_object() { ~fd ? ::close(fd) : 0; }
    } fd_object;

    auto &fd = fd_object.fd;
    fd = do_syscall(::open, MEMINFO_PATH, O_RDONLY);
    if(fd < 0) {
        return -1;
    }

    /// read and parse

    // avoid empty content and real errors
    if(do_syscall(::read, fd, buf, sizeof buf) > 0) {
        ::memset(&meminfo, 0, sizeof (Meminfo));
        const char *cursor = buf;
        size_t index = 0;
        while(index < MEMINFO_TYPES && (cursor = meminfo_parse_line(meminfo, cursor, index++)));
        // if successful, return 0
        if(index != MEMINFO_TYPES) {
            log(__FUNCTION__, ": failed at index ", index);
        }
        return index == MEMINFO_TYPES ? 0 : -1;
    }
    return -1;
}

inline const char* meminfo_parse_line(Meminfo &meminfo, const char *cursor, size_t arr_index) {
    if(arr_index >= MEMINFO_TYPES) {
        log(__FUNCTION__, " out of index: ", arr_index);
        return nullptr;
    }
    const char *meminfo_name = meminfo_names[arr_index];

    auto find_first_not = [](const char *text, auto &&...functors) {
        while(text && (functors(*text) || ...)) text++;
        return text;
    };
    auto find_first = [&](const char *text, auto &&...functors) {
        auto and_not = [&](auto c) { return (!functors(c) && ...);};
        return find_first_not(text, and_not);
    };

    auto iscolon = [](char c) { return c == ':'; };
    auto isline = [](char c) { return c == '\n' || c == '\r'; };

    // {MemTotal}, pos_colon
    auto pos_colon = find_first(cursor, iscolon);
    // {: }, pos_val
    auto pos_val = find_first_not(pos_colon, iscolon, ::isblank);

    bool verified {
        pos_colon &&
        pos_val &&
        !::strncmp(meminfo_name, cursor, strlen(meminfo_name))
    };

    if(verified) {
        ::sscanf(pos_val, "%lld", &meminfo.arr[arr_index]);

        // {123 kB} or {0}
        auto nextline = find_first(pos_val, isline);
        // touch EOF or {\n}
        if(nextline && isline(*nextline)) nextline++;
        // nextline or EOF
        return nextline;
    }

    log(__FUNCTION__, " verified error: [colon, val] = ", !!pos_colon, !!pos_val);
    return nullptr;
}

inline void dump(Meminfo &meminfo, std::ostream &os) {
    for(size_t i = 0; i < MEMINFO_TYPES; ++i) {
        os << meminfo_names[i] << ":\t" << meminfo.arr[i] << " kB" << std::endl;
    }
}

inline void dump_diff(Meminfo &before, Meminfo &after, Meminfo_field_type diff_kb, std::ostream &os) {
    if(diff_kb < 0) diff_kb = -diff_kb;
    for(size_t i = 0; i < MEMINFO_TYPES; ++i) {
        auto calc = after.arr[i] - before.arr[i];
        if(calc > diff_kb || calc < -diff_kb) {
            char op = calc > 0 ? '+' : (calc = -calc, '-');
            os << op << meminfo_names[i] << ":\t" << calc << " kB" << std::endl;
        }
    }
}

} // namespace mp
