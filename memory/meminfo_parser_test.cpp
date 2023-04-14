#include "meminfo_parser.hpp"
#include <vector>

int main() {
    mp::Meminfo before, after;
    if(!mp::meminfo_parse(before)) {
        mp::dump(before);

        constexpr size_t MAXN = 1e7;
        // auto optimized_out= new char[MAXN];
        std::vector<uint64_t> huge(MAXN);
        // page fault
        for(auto &h : huge) huge[0] += h;
        // avoid optimized-out
        std::cout << "====================" << (huge[0] > 123 ? '=' : ' ') << std::endl;

        if(!mp::meminfo_parse(after)) {
            mp::dump_diff(before, after, 100);
        }
    }
    return 0;
}
