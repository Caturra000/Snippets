#include <cstdint>
#include <cstddef>
#include <emmintrin.h>

constexpr size_t N = 1 << 26;

alignas(64) uint32_t a[N];

void test_prefetch(size_t n, size_t times) {
    while(times--) for(size_t i = 0; i < n; ++i) {
        // Case 1:
        _mm_prefetch(&a[i], _MM_HINT_T0);

        // Case 2:
        // volatile int x = 0;

        // Case 3 (need to modify asm code):
        /* nop; */

        // Case 4:
        // volatile int x = a[0];

        // Case 5:
        // volatile int x = a[i];

        // Case 6:
        // a[i] = (volatile int)i;
    }
}

int main() {
    test_prefetch(N, 1024);
}

/*
# We use clang, not gcc. The former unrolls loops by default, which makes the results easier to observe.
# caole,chongzhuangxitonghoulianzhongwendoumeifashuru.
clang++-18 -O3 from prefetch.cpp or prefetch.s

sudo perf stat -e cache-references,ls_sw_pf_dc_fills.all,l2_pf_hit_l2.all,ls_pref_instr_disp.prefetch,instructions ./a.out

==========

prefetch
 Performance counter stats for './a.out':

         2,797,582      cache-references                                                      
            25,892      ls_sw_pf_dc_fills.all                                                 
           250,870      l2_pf_hit_l2.all                                                      
    68,719,921,295      ls_pref_instr_disp.prefetch                                           
    94,549,479,290      instructions                                                          

       4.642557095 seconds time elapsed

       4.641046000 seconds user
       0.001000000 seconds sys        


plain load
 Performance counter stats for './a.out':

         3,678,129      cache-references                                                      
               617      ls_sw_pf_dc_fills.all                                                 
           308,108      l2_pf_hit_l2.all                                                      
             2,993      ls_pref_instr_disp.prefetch                                           
    85,991,333,387      instructions                                                          

       7.565156941 seconds time elapsed

       7.561902000 seconds user
       0.001999000 seconds sys
=> We found that mm_prefetch is not a heavy weight operation.
=> Software prefetch works (in the previous case), but not all mm_prefetchs are effective.
=> All mm_prefetchs are dispatched (ls_pref_instr_disp) and executed (instructions).


no load
 Performance counter stats for './a.out':

         1,142,503      cache-references                                                      
               455      ls_sw_pf_dc_fills.all                                                 
            90,832      l2_pf_hit_l2.all                                                      
             1,840      ls_pref_instr_disp.prefetch                                           
    17,203,802,252      instructions                                                          

       1.707077977 seconds time elapsed

       1.704724000 seconds user
       0.001999000 seconds sys
=> mm_prefetchs incur some cache-references.
=> Since there are no-ops, the number of instructions is explicitly less than the previous one.
=> No extra cache-references if we are playing with registers only.


forced load a[0]
 Performance counter stats for './a.out':

         4,029,146      cache-references                                                      
               576      ls_sw_pf_dc_fills.all                                                 
           359,393      l2_pf_hit_l2.all                                                      
             4,244      ls_pref_instr_disp.prefetch                                           
    85,989,664,328      instructions                                                          

       6.851261751 seconds time elapsed

       6.850276000 seconds user
       0.000000000 seconds sys


forced load a[i]
 Performance counter stats for './a.out':

     5,445,923,238      cache-references                                                      
               969      ls_sw_pf_dc_fills.all                                                 
     3,782,727,264      l2_pf_hit_l2.all                                                      
            69,880      ls_pref_instr_disp.prefetch                                           
   189,260,149,011      instructions                                                          

      11.178864024 seconds time elapsed

      11.138451000 seconds user
       0.037994000 seconds sys
=> Many cache-references!
=> We need to take a look at the ASM code, it might be load + load, which makes instructions large.


forced store a[i]
 Performance counter stats for './a.out':

     8,786,726,243      cache-references                                                      
            64,539      ls_sw_pf_dc_fills.all                                                 
     2,886,108,285      l2_pf_hit_l2.all                                                      
           136,910      ls_pref_instr_disp.prefetch                                           
    60,707,488,618      instructions                                                          

      11.203631235 seconds time elapsed

      11.115490000 seconds user
       0.084980000 seconds sys
=> Many many cache-references!
=> More ls_sw_pf_dc_fills than we expected.
*/