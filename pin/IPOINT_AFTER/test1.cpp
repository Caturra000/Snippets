#include <stdio.h>

constexpr bool PRINTF_CALL = true;
int nop(const char*, ...) { return 0; }

constexpr auto opt_printf = PRINTF_CALL ? printf : nop;



// 需要分析 jmp 的情况，
// 在 g++-14 的 -O3 && !PRINTF_CALL 场景下，
// [main->{instrument_me]->dummy_function} 过程中，[] 过程是 call 调用，而 {} 会被优化为 jmp 尾调用
// 不清楚 pin 对于这种情况是怎么看的

[[gnu::noinline]]
void dummy_function() {
    printf("    -> inside dummy_function()\n");
}

void instrument_me(int value) {
    opt_printf("\n--- Entering instrument_me with value = %d ---\n", value);

    if (value > 5) {
        opt_printf("  [APP] value is > 5. Calling dummy_function.\n");
        dummy_function();
    } else {
        opt_printf("  [APP] value is <= 5.\n");
    }

    opt_printf("--- Exiting instrument_me ---\n");
}

int main() {
    opt_printf("Calling instrument_me(10) to test the FALL-THROUGH path...\n");
    instrument_me(10); // if

    opt_printf("\nCalling instrument_me(3) to test the TAKEN-BRANCH path...\n");
    instrument_me(3);  // else

    return 0;
}
