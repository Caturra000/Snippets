#include <bits/stdc++.h>
constexpr size_t YIELD_MAX = 1e8;

int main() {
    for(size_t i = 0; i < YIELD_MAX; ++i) {
        std::this_thread::yield();
    }
    return 0;
}

/*
 Performance counter stats for './a.out':

          10955.58 msec task-clock:u              #    1.000 CPUs utilized          
                 0      context-switches:u        #    0.000 /sec                   
                 0      cpu-migrations:u          #    0.000 /sec                   
               120      page-faults:u             #   10.953 /sec                   
        8244945679      cycles:u                  #    0.753 GHz                    
          13653393      stalled-cycles-frontend:u #    0.17% frontend cycles idle   
         574435564      stalled-cycles-backend:u  #    6.97% backend cycles idle    
        2402168974      instructions:u            #    0.29  insn per cycle         
                                                  #    0.24  stalled cycles per insn
        1000339456      branches:u                #   91.309 M/sec                  
         100076010      branch-misses:u           #   10.00% of all branches        

      10.956858346 seconds time elapsed

       2.511358000 seconds user
       8.444568000 seconds sys
*/

/*

可以看出和耗时在内核态，即调度器的yield实现上

# Overhead  Command  Shared Object         Symbol                                       
# ........  .......  ....................  .............................................
#
    18.79%  a.out    [kernel.kallsyms]     [k] read_hv_clock_tsc
    14.67%  a.out    [kernel.kallsyms]     [k] __entry_text_start
    11.59%  a.out    libc.so.6             [.] __sched_yield
    10.77%  a.out    [kernel.kallsyms]     [k] __sched_text_start
     5.85%  a.out    [kernel.kallsyms]     [k] yield_task_fair
     5.40%  a.out    [kernel.kallsyms]     [k] do_sched_yield
     3.26%  a.out    [kernel.kallsyms]     [k] _raw_spin_lock
     3.14%  a.out    [kernel.kallsyms]     [k] schedule
     2.96%  a.out    [kernel.kallsyms]     [k] entry_SYSCALL_64_after_hwframe
     2.57%  a.out    a.out                 [.] main
     2.55%  a.out    a.out                 [.] _ZL15__gthread_yieldv
     1.95%  a.out    [kernel.kallsyms]     [k] __raw_callee_save___pv_queued_spin_unlock
     1.94%  a.out    [kernel.kallsyms]     [k] update_rq_clock
     1.73%  a.out    [kernel.kallsyms]     [k] pick_next_task_fair
     1.60%  a.out    [kernel.kallsyms]     [k] syscall_return_via_sysret
     1.41%  a.out    [kernel.kallsyms]     [k] update_curr
     1.33%  a.out    [kernel.kallsyms]     [k] entry_SYSCALL_64_safe_stack
     1.20%  a.out    [kernel.kallsyms]     [k] syscall_exit_to_user_mode
     0.94%  a.out    [kernel.kallsyms]     [k] pick_next_entity
     0.89%  a.out    [kernel.kallsyms]     [k] sched_clock_cpu
     0.84%  a.out    [kernel.kallsyms]     [k] syscall_enter_from_user_mode
     0.67%  a.out    [kernel.kallsyms]     [k] do_syscall_64
     0.52%  a.out    [kernel.kallsyms]     [k] exit_to_user_mode_prepare
     0.50%  a.out    [kernel.kallsyms]     [k] cpuacct_charge
     0.47%  a.out    [kernel.kallsyms]     [k] __list_del_entry_valid
     0.42%  a.out    [kernel.kallsyms]     [k] update_min_vruntime
     0.42%  a.out    [kernel.kallsyms]     [k] __x64_sys_sched_yield
     0.29%  a.out    [kernel.kallsyms]     [k] __list_add_valid
     0.23%  a.out    a.out                 [.] _ZNSt11this_thread5yieldEv
     0.22%  a.out    [kernel.kallsyms]     [k] read_hv_sched_clock_tsc
     0.21%  a.out    [kernel.kallsyms]     [k] rcu_qs
     0.20%  a.out    [kernel.kallsyms]     [k] rcu_note_context_switch
     0.20%  a.out    [kernel.kallsyms]     [k] check_cfs_rq_runtime
     0.18%  a.out    [kernel.kallsyms]     [k] __x86_indirect_thunk_rax
     0.01%  a.out    [kernel.kallsyms]     [k] asm_sysvec_hyperv_stimer0
     0.01%  a.out    [kernel.kallsyms]     [k] x86_pmu_disable_all
     0.01%  a.out    [kernel.kallsyms]     [k] 0x000049000000b003
     0.00%  a.out    [kernel.kallsyms]     [k] timekeeping_update
     0.00%  a.out    [kernel.kallsyms]     [k] update_vsyscall
     0.00%  a.out    [kernel.kallsyms]     [k] tick_sched_timer
     0.00%  a.out    libgcc_s.so.1         [.] 0x0000000000004974
     0.00%  a.out    [kernel.kallsyms]     [k] arch_scale_freq_tick
     0.00%  a.out    [kernel.kallsyms]     [k] __hrtimer_run_queues
     0.00%  a.out    [kernel.kallsyms]     [k] ktime_get_update_offsets_now
     0.00%  a.out    [kernel.kallsyms]     [k] update_process_times
     0.00%  a.out    [kernel.kallsyms]     [k] __update_load_avg_se
     0.00%  a.out    [kernel.kallsyms]     [k] memcpy_erms
     0.00%  a.out    [kernel.kallsyms]     [k] account_process_tick
     0.00%  a.out    [kernel.kallsyms]     [k] rebalance_domains
     0.00%  a.out    [kernel.kallsyms]     [k] irq_work_tick
     0.00%  a.out    [kernel.kallsyms]     [k] rcu_segcblist_ready_cbs
     0.00%  a.out    [kernel.kallsyms]     [k] __note_gp_changes
     0.00%  a.out    [kernel.kallsyms]     [k] hv_ce_set_next_event
     0.00%  a.out    libstdc++.so.6.0.30   [.] 0x00000000000a12e4
     0.00%  a.out    ld-linux-x86-64.so.2  [.] _dl_relocate_object
     0.00%  a.out    ld-linux-x86-64.so.2  [.] _dl_lookup_symbol_x
     0.00%  a.out    [kernel.kallsyms]     [k] down_write
     0.00%  a.out    [kernel.kallsyms]     [k] __task_pid_nr_ns
     0.00%  a.out    [kernel.kallsyms]     [k] move_page_tables
     0.00%  perf-ex  [kernel.kallsyms]     [k] rcu_read_unlock_strict
     0.00%  perf-ex  [kernel.kallsyms]     [k] mutex_lock
*/
