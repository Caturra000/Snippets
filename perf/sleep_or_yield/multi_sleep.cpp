#include <bits/stdc++.h>
constexpr size_t SLEEP_MAX = 1e8;
constexpr size_t THREAD_MAX = 10;

int main() {
    auto func = [] {
        using namespace std::chrono_literals;
        constexpr auto _0_forced = 0ms;
        for(size_t i = 0; i < SLEEP_MAX; ++i) {
            std::this_thread::sleep_for(_0_forced);
        }
    };

    std::vector<std::jthread> threads;
    for(size_t _ = THREAD_MAX; _--; ) threads.emplace_back(func);
    return 0;
}

/*
 Performance counter stats for './a.out':

          19437.84 msec task-clock                #    9.751 CPUs utilized          
                41      context-switches          #    2.109 /sec                   
                21      cpu-migrations            #    1.080 /sec                   
               150      page-faults               #    7.717 /sec                   
       71838254948      cycles                    #    3.696 GHz                    
         180362777      stalled-cycles-frontend   #    0.25% frontend cycles idle   
           8689084      stalled-cycles-backend    #    0.01% backend cycles idle    
      138011359262      instructions              #    1.92  insn per cycle         
                                                  #    0.00  stalled cycles per insn
       24002152388      branches                  #    1.235 G/sec                  
            200997      branch-misses             #    0.00% of all branches        

       1.993371871 seconds time elapsed

      19.396718000 seconds user
       0.000000000 seconds sys
*/

/*
# Overhead  Command  Shared Object         Symbol                                                                                 
# ........  .......  ....................  .......................................................................................
#
    22.49%  a.out    a.out                 [.] _ZNSt6chrono8durationIlSt5ratioILl1ELl1000EEEC1IlvEERKT_
    15.64%  a.out    a.out                 [.] _ZNKSt6chrono8durationIlSt5ratioILl1ELl1000EEE5countEv
    14.32%  a.out    a.out                 [.] _ZNSt6chrono8durationIlSt5ratioILl1ELl1000EEE4zeroEv
    10.53%  a.out    a.out                 [.] _ZStleSt15strong_orderingNSt9__cmp_cat8__unspecE
     9.92%  a.out    a.out                 [.] _ZZ4mainENKUlvE_clEv
     8.86%  a.out    a.out                 [.] _ZNSt6chronossIlSt5ratioILl1ELl1000EElS2_EEDaRKNS_8durationIT_T0_EERKNS3_IT1_T2_EE
     8.41%  a.out    a.out                 [.] _ZNSt11this_thread9sleep_forIlSt5ratioILl1ELl1000EEEEvRKNSt6chrono8durationIT_T0_EE
     6.44%  a.out    a.out                 [.] _ZNSt6chrono15duration_valuesIlE4zeroEv
     3.33%  a.out    a.out                 [.] _ZNSt9__cmp_cat8__unspecC2EPS0_
     0.01%  a.out    [kernel.kallsyms]     [k] asm_sysvec_hyperv_stimer0
     0.00%  a.out    [kernel.kallsyms]     [k] x86_pmu_disable_all
     0.00%  a.out    [kernel.kallsyms]     [k] hv_ce_set_next_event
     0.00%  a.out    [kernel.kallsyms]     [k] memcpy_erms
*/
