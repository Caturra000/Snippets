#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/processor.h>
#include <asm/msr.h>
#include <asm/cpufeature.h>

#define MODULE_NAME "amd_uai_enabler"

// AMD CPUID function for UAI detection
#define UAI_CPUID_FN      0x80000021
#define UAI_CPUID_BIT     (1 << 7)  // EAX bit 7

// EFER MSR definitions
#ifndef MSR_EFER
#define MSR_EFER          0xC0000080
#endif
#define EFER_UAIE_BIT     (1 << 20) // Bit 20

static bool uai_supported = false;

// 检查单个CPU是否支持UAI
static bool check_cpu_uai_support(unsigned int cpu)
{
    struct cpuinfo_x86 *c = &cpu_data(cpu);
    u32 eax, ebx, ecx, edx;
    
    // 检查是否支持扩展CPUID函数
    if (c->extended_cpuid_level < UAI_CPUID_FN) {
        pr_info("CPU%d: UAI CPUID function not supported\n", cpu);
        return false;
    }
    
    cpuid_count(UAI_CPUID_FN, 0, &eax, &ebx, &ecx, &edx);
    
    if (eax & UAI_CPUID_BIT) {
        pr_info("CPU%d: UAI supported\n", cpu);
        return true;
    }
    
    pr_info("CPU%d: UAI not supported\n", cpu);
    return false;
}

// 在单个CPU上启用UAI
static void enable_uai_on_cpu(void *unused)
{
    u64 efer;
    int cpu = smp_processor_id();
    
    if (!check_cpu_uai_support(cpu)) {
        pr_warn("CPU%d: Cannot enable UAI - not supported\n", cpu);
        return;
    }
    
    rdmsrl(MSR_EFER, efer);
    
    if (efer & EFER_UAIE_BIT) {
        pr_info("CPU%d: UAI already enabled (EFER=0x%llx)\n", cpu, efer);
        return;
    }
    
    wrmsrl(MSR_EFER, efer | EFER_UAIE_BIT);
    rdmsrl(MSR_EFER, efer); // 验证
    
    if (efer & EFER_UAIE_BIT) {
        pr_info("CPU%d: UAI enabled successfully (EFER=0x%llx)\n", cpu, efer);
    } else {
        pr_err("CPU%d: Failed to enable UAI (EFER=0x%llx)\n", cpu, efer);
    }
}

// 热插拔CPU回调
static int uai_cpu_online(unsigned int cpu)
{
    if (uai_supported) {
        smp_call_function_single(cpu, enable_uai_on_cpu, NULL, 1);
    }
    return 0;
}

static int uai_cpu_down_prep(unsigned int cpu)
{
    // 不需要特别操作
    return 0;
}

// 检查所有CPU的支持性
static bool check_all_cpus_support(void)
{
    unsigned int cpu;
    
    for_each_online_cpu(cpu) {
        if (!check_cpu_uai_support(cpu)) {
            pr_err("UAI not supported on all CPUs\n");
            return false;
        }
    }
    return true;
}

// 热插拔回调状态
static enum cpuhp_state uai_hp_state;

// 模块初始化
static int __init uai_enabler_init(void)
{
    int ret;
    pr_info("AMD UAI Enabler Module Loading\n");
    
    // 检查所有CPU是否支持UAI
    if (!check_all_cpus_support()) {
        pr_err("System does not support UAI on all CPUs\n");
        return -ENODEV;
    }
    
    uai_supported = true;
    
    // 在所有在线CPU上启用UAI
    on_each_cpu(enable_uai_on_cpu, NULL, 1);
    
    // 注册热插拔回调
    ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "uai:online",
                                uai_cpu_online, uai_cpu_down_prep);
    
    if (ret < 0) {
        pr_err("Failed to register CPU hotplug callback: %d\n", ret);
        return ret;
    }
    
    uai_hp_state = ret;
    pr_info("UAI enabled on all CPUs and hotplug registered\n");
    
    return 0;
}

// 模块退出
static void __exit uai_enabler_exit(void)
{
    if (uai_supported) {
        // 注销热插拔回调
        cpuhp_remove_state(uai_hp_state);
        pr_info("CPU hotplug callback unregistered\n");
        
        // 注意：通常不应禁用UAI，因为其他组件可能依赖它
        // 这里仅显示如何禁用的示例（实际使用不建议）
        pr_info("UAI remains enabled on all CPUs\n");
    }
    
    pr_info("AMD UAI Enabler Module Unloaded\n");
}

module_init(uai_enabler_init);
module_exit(uai_enabler_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("滴扑息克");
MODULE_DESCRIPTION("AMD Upper Address Ignore (UAI) Enabler");
MODULE_VERSION("1.0");