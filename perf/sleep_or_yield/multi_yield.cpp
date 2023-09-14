#include <bits/stdc++.h>
constexpr size_t YIELD_MAX = 1e8;
constexpr size_t THREAD_MAX = 10;

int main() {
    auto func = [] {
        for(size_t i = 0; i < YIELD_MAX; ++i) {
            std::this_thread::yield();
        }
    };
    std::vector<std::jthread> threads;
    for(size_t _ = THREAD_MAX; _--;) threads.emplace_back(func);
    return 0;
}

/*
 Performance counter stats for './a.out':

         163605.71 msec task-clock                #    9.953 CPUs utilized          
                98      context-switches          #    0.599 /sec                   
                39      cpu-migrations            #    0.238 /sec                   
               152      page-faults               #    0.929 /sec                   
      619188799186      cycles                    #    3.785 GHz                    
         699208759      stalled-cycles-frontend   #    0.11% frontend cycles idle   
       13616505600      stalled-cycles-backend    #    2.20% backend cycles idle    
      683052400076      instructions              #    1.10  insn per cycle         
                                                  #    0.02  stalled cycles per insn
      162010499150      branches                  #  990.250 M/sec                  
        4003458937      branch-misses             #    2.47% of all branches        

      16.437133396 seconds time elapsed

      38.969891000 seconds user
     124.651639000 seconds sys
*/

/*
# Overhead  Command  Shared Object         Symbol                                       
# ........  .......  ....................  .............................................
#
    14.86%  a.out    [kernel.kallsyms]     [k] read_hv_clock_tsc
    13.02%  a.out    [kernel.kallsyms]     [k] __entry_text_start
    11.47%  a.out    libc.so.6             [.] __sched_yield
     9.21%  a.out    [kernel.kallsyms]     [k] __sched_text_start
     4.91%  a.out    [kernel.kallsyms]     [k] do_sched_yield
     4.50%  a.out    [kernel.kallsyms]     [k] yield_task_fair
     3.74%  a.out    [kernel.kallsyms]     [k] pick_next_task_fair
     3.39%  a.out    [kernel.kallsyms]     [k] syscall_return_via_sysret
     3.27%  a.out    [kernel.kallsyms]     [k] entry_SYSCALL_64_after_hwframe
     2.82%  a.out    [kernel.kallsyms]     [k] _raw_spin_lock
     2.36%  a.out    [kernel.kallsyms]     [k] schedule
     2.22%  a.out    [kernel.kallsyms]     [k] __raw_callee_save___pv_queued_spin_unlock
     2.16%  a.out    a.out                 [.] _ZZ4mainENKUlvE_clEv
     1.87%  a.out    a.out                 [.] _ZL15__gthread_yieldv
     1.80%  a.out    [kernel.kallsyms]     [k] __list_add_valid
     1.79%  a.out    [kernel.kallsyms]     [k] syscall_exit_to_user_mode
     1.79%  a.out    [kernel.kallsyms]     [k] update_rq_clock
     1.77%  a.out    [kernel.kallsyms]     [k] update_curr
     1.61%  a.out    [kernel.kallsyms]     [k] pick_next_entity
     1.43%  a.out    [kernel.kallsyms]     [k] exit_to_user_mode_prepare
     1.37%  a.out    [kernel.kallsyms]     [k] entry_SYSCALL_64_safe_stack
     1.30%  a.out    [kernel.kallsyms]     [k] __list_del_entry_valid
     1.27%  a.out    [kernel.kallsyms]     [k] do_syscall_64
     1.13%  a.out    [kernel.kallsyms]     [k] syscall_enter_from_user_mode
     1.07%  a.out    [kernel.kallsyms]     [k] __x64_sys_sched_yield
     0.87%  a.out    [kernel.kallsyms]     [k] update_min_vruntime
     0.79%  a.out    [kernel.kallsyms]     [k] cpuacct_charge
     0.68%  a.out    [kernel.kallsyms]     [k] sched_clock_cpu
     0.29%  a.out    [kernel.kallsyms]     [k] rcu_note_context_switch
     0.28%  a.out    [kernel.kallsyms]     [k] read_hv_sched_clock_tsc
     0.26%  a.out    a.out                 [.] _ZNSt11this_thread5yieldEv
     0.23%  a.out    [kernel.kallsyms]     [k] rcu_qs
     0.19%  a.out    [kernel.kallsyms]     [k] __x86_indirect_thunk_rax
     0.14%  a.out    [kernel.kallsyms]     [k] check_cfs_rq_runtime
     0.06%  a.out    [kernel.kallsyms]     [k] sched_clock
     0.02%  a.out    [kernel.kallsyms]     [k] rcu_read_unlock_st
*/
