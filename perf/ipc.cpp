// 需要-O0编译
int main() {
    unsigned long i {}, sum {};
    for(; i < 0x10000000; i++) {
        sum += i;
    }
}

// 5800H测试数据：
// $ perf stat ./ipc
//  Performance counter stats for './ipc':

//             181.61 msec task-clock                #    0.998 CPUs utilized          
//                  0      context-switches          #    0.000 /sec                   
//                  0      cpu-migrations            #    0.000 /sec                   
//                 51      page-faults               #  280.822 /sec                   
//          774429218      cycles                    #    4.264 GHz                    
//             216051      stalled-cycles-frontend   #    0.03% frontend cycles idle   
//              60190      stalled-cycles-backend    #    0.01% backend cycles idle    
//         1342786806      instructions              #    1.73  insn per cycle         
//                                                   #    0.00  stalled cycles per insn
//          268559344      branches                  #    1.479 G/sec                  
//               7396      branch-misses             #    0.00% of all branches        

//        0.181905472 seconds time elapsed

//        0.181978000 seconds user
//        0.000000000 seconds sys
