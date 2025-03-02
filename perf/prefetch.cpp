#include <cstdint>
#include <cstddef>
#include <emmintrin.h>

constexpr size_t N = 1 << 26;

alignas(64) uint32_t a[N];

void test_prefetch(size_t n, size_t times) {
    while(times--) for(size_t i = 0; i < n; ++i) {
        // Case 1:
        // _mm_prefetch(&a[i], _MM_HINT_T0);
        // Case 2:
        // volatile int x = 0;
        // Case 3 (need to modify asm code):
        /* nop; */
        // Case 4:
        // bolatile int x = a[0];
    }
}

int main() {
    test_prefetch(N, 1024);
}

/*
# We use clang, not gcc. The former unrolls loops by default, which makes the results easier to observe.
# caole,chongzhuangxitonghoulianzhongwendoumeifashuru.
clang++-18 -O3 from prefetch.cpp or prefetch.s

==========

prefetch
 Performance counter stats for './a.out':

         2,608,978      cache-references                                                      
            21,354      ls_sw_pf_dc_fills.all                                                 
           294,543      l2_pf_hit_l2.all                                                      
    94,547,376,456      instructions             

plain load
 Performance counter stats for './b.out':

         4,138,209      cache-references                                                      
               699      ls_sw_pf_dc_fills.all                                                 
           262,819      l2_pf_hit_l2.all                                                      
    85,994,283,992      instructions                                                          

       7.561222860 seconds time elapsed

       7.559342000 seconds user
       0.000999000 seconds sys

no load
           864,535      cache-references                                                      
               428      ls_sw_pf_dc_fills.all                                                 
            87,901      l2_pf_hit_l2.all                                                      
    17,203,622,294      instructions                                                          

       1.704277282 seconds time elapsed

       1.703881000 seconds user
       0.000000000 seconds sys

forced load a[0]
         4,165,118      cache-references                                                      
               590      ls_sw_pf_dc_fills.all                                                 
           288,136      l2_pf_hit_l2.all                                                      
    85,989,338,223      instructions                                                          

       6.844549754 seconds time elapsed

       6.844020000 seconds user
       0.000000000 seconds sys
*/