#include <bits/stdc++.h>
constexpr size_t SLEEP_MAX = 1e3;

int main() {
    using namespace std::chrono_literals;
    constexpr auto _1_forced = 1ms;
    for(size_t i = 0; i < SLEEP_MAX; ++i) {
        std::this_thread::sleep_for(_1_forced);
    }
    return 0;
}

/*
如果有非零的时间，那么睡多少次就cs多少次，即使是单线程也不例外
 Performance counter stats for './a.out':

             50.42 msec task-clock                #    0.045 CPUs utilized          
              1000      context-switches          #   19.834 K/sec                  
                 3      cpu-migrations            #   59.502 /sec                   
               122      page-faults               #    2.420 K/sec                  
          19015513      cycles                    #    0.377 GHz                    
           3672123      stalled-cycles-frontend   #   19.31% frontend cycles idle   
           1676138      stalled-cycles-backend    #    8.81% backend cycles idle    
           8470253      instructions              #    0.45  insn per cycle         
                                                  #    0.43  stalled cycles per insn
           1803383      branches                  #   35.768 M/sec                  
            125707      branch-misses             #    6.97% of all branches        

       1.114384568 seconds time elapsed

       0.000000000 seconds user
       0.038326000 seconds sys
*/

/*
不知道为啥会关闭pmu
# Overhead  Command  Shared Object         Symbol                                                                                                                                               >
# ........  .......  ....................  .....................................................................................................................................................>
#
    16.78%  a.out    [kernel.kallsyms]     [k] x86_pmu_disable_all
    10.16%  a.out    [kernel.kallsyms]     [k] hv_ce_set_next_event
     6.70%  a.out    ld-linux-x86-64.so.2  [.] do_lookup_x
     5.77%  a.out    a.out                 [.] _ZNSt11this_thread9sleep_forIlSt5ratioILl1ELl1000EEEEvRKNSt6chrono8durationIT_T0_EE
     4.40%  a.out    [kernel.kallsyms]     [k] pick_next_task_fair
     3.41%  a.out    ld-linux-x86-64.so.2  [.] strcmp
     2.68%  a.out    ld-linux-x86-64.so.2  [.] _dl_cache_libcmp
     2.62%  a.out    [kernel.kallsyms]     [k] exit_to_user_mode_prepare
     2.47%  a.out    libc.so.6             [.] clock_nanosleep@GLIBC_2.2.5
     2.21%  a.out    [kernel.kallsyms]     [k] read_tsc
     1.89%  a.out    [kernel.kallsyms]     [k] hrtimer_nanosleep
     1.78%  a.out    [kernel.kallsyms]     [k] __x64_sys_clock_nanosleep
     1.44%  a.out    [kernel.kallsyms]     [k] clear_buddies
     1.44%  a.out    [kernel.kallsyms]     [k] dequeue_task_fair
     1.42%  a.out    [kernel.kallsyms]     [k] newidle_balance
     1.28%  a.out    [kernel.kallsyms]     [k] switch_fpu_return
     1.26%  a.out    [kernel.kallsyms]     [k] update_curr
     1.24%  a.out    [kernel.kallsyms]     [k] do_nanosleep
     1.23%  a.out    [kernel.kallsyms]     [k] do_syscall_64
*/
