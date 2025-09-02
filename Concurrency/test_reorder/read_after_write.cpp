#include <atomic>
#include <vector>
#include <thread>
#include <iostream>

std::atomic<int> x {0};

void test(int i) {
    x.store(i);
    int y = x.load();
}

/*
g++ -O2编译

如果使用relaxed 或者 acq和rel组合
0000000000001530 <_Z4testi>:
    1530:	f3 0f 1e fa          	endbr64 
    1534:	89 3d 1a 2c 00 00    	mov    %edi,0x2c1a(%rip)        # 4154 <x>
    153a:	8b 05 14 2c 00 00    	mov    0x2c14(%rip),%eax        # 4154 <x>
    1540:	c3                   	retq   
    1541:	66 2e 0f 1f 84 00 00 	nopw   %cs:0x0(%rax,%rax,1)
    1548:	00 00 00 
    154b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

如果使用seq_cst
0000000000001530 <_Z4testi>:
    1530:	f3 0f 1e fa          	endbr64 
    1534:	89 3d 1a 2c 00 00    	mov    %edi,0x2c1a(%rip)        # 4154 <x>
    153a:	0f ae f0             	mfence 
    153d:	8b 05 11 2c 00 00    	mov    0x2c11(%rip),%eax        # 4154 <x>
    1543:	c3                   	retq   
    1544:	66 2e 0f 1f 84 00 00 	nopw   %cs:0x0(%rax,%rax,1)
    154b:	00 00 00 
    154e:	66 90                	xchg   %ax,%ax
*/

int main() {
    std::vector<std::thread> threads;
    for(int i = 1; i < 10; ++i) {
        threads.emplace_back(test, i);
    }
    for(auto &&t : threads) t.join();
    std::cout << x << std::endl;
    return 0;
}
