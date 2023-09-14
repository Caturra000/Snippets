#include <bits/stdc++.h>
constexpr size_t SLEEP_MAX = 1e8;

int main() {
    using namespace std::chrono_literals;
    constexpr auto _0_forced = 0ms;
    for(size_t i = 0; i < SLEEP_MAX; ++i) {
        std::this_thread::sleep_for(_0_forced);
    }
    return 0;
}

/*

 Performance counter stats for './a.out':

           1102.95 msec task-clock:u              #    1.000 CPUs utilized          
                 0      context-switches:u        #    0.000 /sec                   
                 0      cpu-migrations:u          #    0.000 /sec                   
               120      page-faults:u             #  108.799 /sec                   
        4720004540      cycles:u                  #    4.279 GHz                    
           4259940      stalled-cycles-frontend:u #    0.09% frontend cycles idle   
            332219      stalled-cycles-backend:u  #    0.01% backend cycles idle    
       12802170140      instructions:u            #    2.71  insn per cycle         
                                                  #    0.00  stalled cycles per insn
        2100339368      branches:u                #    1.904 G/sec                  
             17403      branch-misses:u           #    0.00% of all branches        

       1.103412099 seconds time elapsed

       1.103418000 seconds user
       0.000000000 seconds sys
*/

/*
热点全在用户态，还是chrono背锅

# Overhead  Command  Shared Object         Symbol                                                                                 
# ........  .......  ....................  .......................................................................................
#
    19.98%  a.out    a.out                 [.] _ZNSt6chrono8durationIlSt5ratioILl1ELl1000EEE4zeroEv
    19.79%  a.out    a.out                 [.] main
    18.79%  a.out    a.out                 [.] _ZNSt6chrono8durationIlSt5ratioILl1ELl1000EEEC1IlvEERKT_
    12.13%  a.out    a.out                 [.] _ZNKSt6chrono8durationIlSt5ratioILl1ELl1000EEE5countEv
    11.55%  a.out    a.out                 [.] _ZNSt6chronoltIlSt5ratioILl1ELl1000EElS2_EEbRKNS_8durationIT_T0_EERKNS3_IT1_T2_EE
     7.24%  a.out    a.out                 [.] _ZNSt11this_thread9sleep_forIlSt5ratioILl1ELl1000EEEEvRKNSt6chrono8durationIT_T0_EE
     6.05%  a.out    a.out                 [.] _ZNSt6chronoleIlSt5ratioILl1ELl1000EElS2_EEbRKNS_8durationIT_T0_EERKNS3_IT1_T2_EE
     4.38%  a.out    a.out                 [.] _ZNSt6chrono15duration_valuesIlE4zeroEv
     0.02%  a.out    [kernel.kallsyms]     [k] ktime_get
     0.02%  a.out    [kernel.kallsyms]     [k] exit_to_user_mode_prepare
     0.01%  a.out    [kernel.kallsyms]     [k] do_brk_flags
     0.01%  a.out    ld-linux-x86-64.so.2  [.] _dl_lookup_symbol_x
     0.01%  a.out    ld-linux-x86-64.so.2  [.] _dl_relocate_object
     0.01%  a.out    [kernel.kallsyms]     [k] __vma_link_rb
     0.00%  a.out    [kernel.kallsyms]     [k] kmem_cache_free
     0.00%  a.out    [kernel.kallsyms]     [k] get_random_u64
     0.00%  perf-ex  [kernel.kallsyms]     [k] perf_event_addr_filters_exec
     0.00%  perf-ex  [kernel.kallsyms]     [k] mutex_lock

如果直接写std::this_thread::sleep_for(0ms);
那么-O0下反汇编是存在call _ZNSt8literals15chrono_literalsli2msIJLc48EEEENSt6chrono8durationIlSt5ratioILl1ELl1000EEEEv操作
filt一下看出是std::chrono::duration<long, std::ratio<1l, 1000l> > std::literals::chrono_literals::operator"" ms<(char)48>()
必须显式用constexpr才能抹掉这个多余的call
至于sleep_for内部剩下的chrono计算我就去不掉了，责任全在编译器
*/
