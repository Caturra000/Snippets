#include <bits/stdc++.h>
constexpr size_t YIELD_MAX = 1e8;
constexpr size_t THREAD_MAX = 10;

void do_something() {
    int jintianxiaomidaobilema = 100;
    // 1. MUST read
    // 2. must read ONCE
    #define read_once(v) *(volatile decltype(v) *)(&v)
    while(jintianxiaomidaobilema = read_once(jintianxiaomidaobilema)-1);
    #undef read_once
}

int main() {
    auto func = [] {
        for(size_t i = 0; i < YIELD_MAX; ++i) {
            do_something();
            using namespace std::chrono_literals;
            std::this_thread::yield();
        }
    };
    std::vector<std::jthread> threads;
    for(size_t _ = THREAD_MAX; _--;) threads.emplace_back(func);
    return 0;
}

/*
 Performance counter stats for './a.out':

         274270.05 msec task-clock                #    9.911 CPUs utilized          
                95      context-switches          #    0.346 /sec                   
                41      cpu-migrations            #    0.149 /sec                   
               150      page-faults               #    0.547 /sec                   
     1019523893133      cycles                    #    3.717 GHz                    
        1123248714      stalled-cycles-frontend   #    0.11% frontend cycles idle   
       17774201794      stalled-cycles-backend    #    1.74% backend cycles idle    
     1599081642008      instructions              #    1.57  insn per cycle         
                                                  #    0.01  stalled cycles per insn
      265016479650      branches                  #  966.261 M/sec                  
        4010626639      branch-misses             #    1.51% of all branches        

      27.673571541 seconds time elapsed

     139.784478000 seconds user
     134.493930000 seconds sys
*/

/*
# Overhead  Command  Shared Object         Symbol                                       
# ........  .......  ....................  .............................................
#
    37.11%  a.out    a.out                 [.] _Z12do_somethingv
     9.26%  a.out    [kernel.kallsyms]     [k] read_hv_clock_tsc
     8.15%  a.out    [kernel.kallsyms]     [k] __entry_text_start
     7.01%  a.out    libc.so.6             [.] __sched_yield
     5.51%  a.out    [kernel.kallsyms]     [k] __sched_text_start
     3.22%  a.out    [kernel.kallsyms]     [k] do_sched_yield
     2.98%  a.out    [kernel.kallsyms]     [k] yield_task_fair
     2.34%  a.out    [kernel.kallsyms]     [k] pick_next_task_fair
     2.24%  a.out    [kernel.kallsyms]     [k] syscall_return_via_sysret
     2.07%  a.out    [kernel.kallsyms]     [k] entry_SYSCALL_64_after_hwframe
     1.75%  a.out    [kernel.kallsyms]     [k] _raw_spin_lock
     1.41%  a.out    [kernel.kallsyms]     [k] schedule
     1.40%  a.out    [kernel.kallsyms]     [k] __raw_callee_save___pv_queued_spin_unlock
     1.36%  a.out    a.out                 [.] _ZZ4mainENKUlvE_clEv
     1.33%  a.out    a.out                 [.] _ZL15__gthread_yieldv
     1.21%  a.out    [kernel.kallsyms]     [k] update_rq_clock
     1.19%  a.out    [kernel.kallsyms]     [k] __list_add_valid
     1.11%  a.out    [kernel.kallsyms]     [k] update_curr
     1.03%  a.out    [kernel.kallsyms]     [k] syscall_exit_to_user_mode
     0.95%  a.out    [kernel.kallsyms]     [k] pick_next_entity
     0.91%  a.out    [kernel.kallsyms]     [k] exit_to_user_mode_prepare
*/
