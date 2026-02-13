#include <benchmark/benchmark.h>
#include <array>
#include <vector>
#include <random>
#include <cstdint>
#include <span>
#include <bit>
#include <concepts>
#include <functional>

//==============================================================================
// 通用定义
//==============================================================================

// 操作码定义 - 涵盖常见VM操作（去掉容易出问题的DIV/MOD）
enum class OpCode : uint8_t {
    HALT = 0,
    NOP,
    // 算术运算
    ADD, SUB, MUL,
    INC, DEC, NEG,
    // 位运算
    AND, OR, XOR, NOT, SHL, SHR,
    // 数据移动
    LOAD_IMM, LOAD_REG, STORE,
    // 比较
    CMP_EQ, CMP_LT, CMP_GT,
    // 控制流
    JMP, JZ, JNZ,
    // 复杂操作
    MAC,     // Multiply-accumulate
    DOT,     // Dot product step
    POLY,    // Polynomial evaluation step
    ABS,     // Absolute value
    MIN,     // Minimum
    MAX,     // Maximum
    
    // 扩展操作码（用于测试更多分支的场景）
    OP32, OP33, OP34, OP35, OP36, OP37, OP38, OP39,
    OP40, OP41, OP42, OP43, OP44, OP45, OP46, OP47,
    OP48, OP49, OP50, OP51, OP52, OP53, OP54, OP55,
    OP56, OP57, OP58, OP59, OP60, OP61, OP62, OP63,
    
    NUM_OPCODES
};

constexpr size_t NUM_REGS = 16;
constexpr size_t NUM_BASIC_OPS = 32;  // HALT到MAX

// VM状态
struct alignas(64) VMState {
    std::array<int64_t, NUM_REGS> regs{};
    uint64_t flags = 0;
    size_t ip = 0;
    uint64_t cycle_count = 0;
    
    void reset() {
        regs.fill(0);
        // 初始化一些非零值，防止某些操作出问题
        for (size_t i = 0; i < NUM_REGS; ++i) {
            regs[i] = static_cast<int64_t>(i + 1);
        }
        flags = 0;
        ip = 0;
        cycle_count = 0;
    }
};

// 编码指令
struct Instruction {
    OpCode op;
    uint8_t r1, r2, r3;  // 寄存器操作数
    int32_t imm;         // 立即数
};

//==============================================================================
// 1. Switch-Case 实现 (传统方式)
//==============================================================================

class SwitchInterpreter {
public:
    [[gnu::noinline]]
    static uint64_t execute(std::span<const Instruction> code, VMState& state) {
        const size_t code_size = code.size();
        
        while (state.ip < code_size) {
            const auto& instr = code[state.ip];
            ++state.cycle_count;
            
            switch (instr.op) {
                case OpCode::HALT:
                    return state.regs[0];
                    
                case OpCode::NOP:
                    ++state.ip;
                    break;
                    
                case OpCode::ADD:
                    state.regs[instr.r1] = state.regs[instr.r2] + state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::SUB:
                    state.regs[instr.r1] = state.regs[instr.r2] - state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::MUL:
                    state.regs[instr.r1] = state.regs[instr.r2] * state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::INC:
                    ++state.regs[instr.r1];
                    ++state.ip;
                    break;
                    
                case OpCode::DEC:
                    --state.regs[instr.r1];
                    ++state.ip;
                    break;
                    
                case OpCode::NEG:
                    state.regs[instr.r1] = -state.regs[instr.r2];
                    ++state.ip;
                    break;
                    
                case OpCode::AND:
                    state.regs[instr.r1] = state.regs[instr.r2] & state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::OR:
                    state.regs[instr.r1] = state.regs[instr.r2] | state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::XOR:
                    state.regs[instr.r1] = state.regs[instr.r2] ^ state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::NOT:
                    state.regs[instr.r1] = ~state.regs[instr.r2];
                    ++state.ip;
                    break;
                    
                case OpCode::SHL:
                    state.regs[instr.r1] = state.regs[instr.r2] << (state.regs[instr.r3] & 63);
                    ++state.ip;
                    break;
                    
                case OpCode::SHR:
                    state.regs[instr.r1] = static_cast<uint64_t>(state.regs[instr.r2]) >> 
                                          (state.regs[instr.r3] & 63);
                    ++state.ip;
                    break;
                    
                case OpCode::LOAD_IMM:
                    state.regs[instr.r1] = instr.imm;
                    ++state.ip;
                    break;
                    
                case OpCode::LOAD_REG:
                    state.regs[instr.r1] = state.regs[instr.r2];
                    ++state.ip;
                    break;
                    
                case OpCode::STORE:
                    state.regs[instr.r1] = state.regs[instr.r2];
                    ++state.ip;
                    break;
                    
                case OpCode::CMP_EQ:
                    state.flags = (state.regs[instr.r2] == state.regs[instr.r3]) ? 1 : 0;
                    ++state.ip;
                    break;
                    
                case OpCode::CMP_LT:
                    state.flags = (state.regs[instr.r2] < state.regs[instr.r3]) ? 1 : 0;
                    ++state.ip;
                    break;
                    
                case OpCode::CMP_GT:
                    state.flags = (state.regs[instr.r2] > state.regs[instr.r3]) ? 1 : 0;
                    ++state.ip;
                    break;
                    
                case OpCode::JMP:
                    state.ip = static_cast<size_t>(instr.imm);
                    break;
                    
                case OpCode::JZ:
                    state.ip = (state.flags == 0) ? 
                               static_cast<size_t>(instr.imm) : state.ip + 1;
                    break;
                    
                case OpCode::JNZ:
                    state.ip = (state.flags != 0) ? 
                               static_cast<size_t>(instr.imm) : state.ip + 1;
                    break;
                    
                case OpCode::MAC:  // r1 += r2 * r3
                    state.regs[instr.r1] += state.regs[instr.r2] * state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::DOT:  // Dot product accumulation
                    state.regs[instr.r1] += state.regs[instr.r2] * state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::POLY: // Horner's method step
                    state.regs[instr.r1] = state.regs[instr.r1] * state.regs[instr.r2] + 
                                           state.regs[instr.r3];
                    ++state.ip;
                    break;
                    
                case OpCode::ABS:
                    state.regs[instr.r1] = std::abs(state.regs[instr.r2]);
                    ++state.ip;
                    break;
                    
                case OpCode::MIN:
                    state.regs[instr.r1] = std::min(state.regs[instr.r2], state.regs[instr.r3]);
                    ++state.ip;
                    break;
                    
                case OpCode::MAX:
                    state.regs[instr.r1] = std::max(state.regs[instr.r2], state.regs[instr.r3]);
                    ++state.ip;
                    break;
                    
                // 扩展操作码 - 简单混合操作
                default:
                    if (instr.op >= OpCode::OP32 && instr.op < OpCode::NUM_OPCODES) {
                        auto op_idx = static_cast<uint8_t>(instr.op) - static_cast<uint8_t>(OpCode::OP32);
                        state.regs[instr.r1] = (state.regs[instr.r2] ^ op_idx) + state.regs[instr.r3];
                    }
                    ++state.ip;
                    break;
            }
        }
        return state.regs[0];
    }
};

//==============================================================================
// 2. Direct Threaded Code (DTC) - 使用计算goto
//==============================================================================

class DirectThreadedInterpreter {
public:
    [[gnu::noinline]]
    static uint64_t execute(std::span<const Instruction> code, VMState& state) {
        #ifdef __GNUC__
        
        // 标签地址表
        static constexpr void* dispatch_table[] = {
            &&L_HALT, &&L_NOP,
            &&L_ADD, &&L_SUB, &&L_MUL,
            &&L_INC, &&L_DEC, &&L_NEG,
            &&L_AND, &&L_OR, &&L_XOR, &&L_NOT, &&L_SHL, &&L_SHR,
            &&L_LOAD_IMM, &&L_LOAD_REG, &&L_STORE,
            &&L_CMP_EQ, &&L_CMP_LT, &&L_CMP_GT,
            &&L_JMP, &&L_JZ, &&L_JNZ,
            &&L_MAC, &&L_DOT, &&L_POLY, &&L_ABS, &&L_MIN, &&L_MAX,
            // 扩展操作码 - 都指向L_EXT
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
        };
        
        const Instruction* ip = code.data();
        const Instruction* code_end = code.data() + code.size();
        auto& regs = state.regs;
        uint64_t flags = 0;
        
        #define DISPATCH() \
            do { \
                if (ip >= code_end) return regs[0]; \
                ++state.cycle_count; \
                goto *dispatch_table[static_cast<size_t>(ip->op)]; \
            } while(0)
        
        #define NEXT() do { ++ip; DISPATCH(); } while(0)
        
        DISPATCH();
        
        L_HALT:
            return regs[0];
            
        L_NOP:
            NEXT();
            
        L_ADD:
            regs[ip->r1] = regs[ip->r2] + regs[ip->r3];
            NEXT();
            
        L_SUB:
            regs[ip->r1] = regs[ip->r2] - regs[ip->r3];
            NEXT();
            
        L_MUL:
            regs[ip->r1] = regs[ip->r2] * regs[ip->r3];
            NEXT();
            
        L_INC:
            ++regs[ip->r1];
            NEXT();
            
        L_DEC:
            --regs[ip->r1];
            NEXT();
            
        L_NEG:
            regs[ip->r1] = -regs[ip->r2];
            NEXT();
            
        L_AND:
            regs[ip->r1] = regs[ip->r2] & regs[ip->r3];
            NEXT();
            
        L_OR:
            regs[ip->r1] = regs[ip->r2] | regs[ip->r3];
            NEXT();
            
        L_XOR:
            regs[ip->r1] = regs[ip->r2] ^ regs[ip->r3];
            NEXT();
            
        L_NOT:
            regs[ip->r1] = ~regs[ip->r2];
            NEXT();
            
        L_SHL:
            regs[ip->r1] = regs[ip->r2] << (regs[ip->r3] & 63);
            NEXT();
            
        L_SHR:
            regs[ip->r1] = static_cast<uint64_t>(regs[ip->r2]) >> (regs[ip->r3] & 63);
            NEXT();
            
        L_LOAD_IMM:
            regs[ip->r1] = ip->imm;
            NEXT();
            
        L_LOAD_REG:
            regs[ip->r1] = regs[ip->r2];
            NEXT();
            
        L_STORE:
            regs[ip->r1] = regs[ip->r2];
            NEXT();
            
        L_CMP_EQ:
            flags = (regs[ip->r2] == regs[ip->r3]) ? 1 : 0;
            NEXT();
            
        L_CMP_LT:
            flags = (regs[ip->r2] < regs[ip->r3]) ? 1 : 0;
            NEXT();
            
        L_CMP_GT:
            flags = (regs[ip->r2] > regs[ip->r3]) ? 1 : 0;
            NEXT();
            
        L_JMP:
            ip = code.data() + ip->imm;
            DISPATCH();
            
        L_JZ:
            ip = (flags == 0) ? code.data() + ip->imm : ip + 1;
            DISPATCH();
            
        L_JNZ:
            ip = (flags != 0) ? code.data() + ip->imm : ip + 1;
            DISPATCH();
            
        L_MAC:
            regs[ip->r1] += regs[ip->r2] * regs[ip->r3];
            NEXT();
            
        L_DOT:
            regs[ip->r1] += regs[ip->r2] * regs[ip->r3];
            NEXT();
            
        L_POLY:
            regs[ip->r1] = regs[ip->r1] * regs[ip->r2] + regs[ip->r3];
            NEXT();
            
        L_ABS:
            regs[ip->r1] = std::abs(regs[ip->r2]);
            NEXT();
            
        L_MIN:
            regs[ip->r1] = std::min(regs[ip->r2], regs[ip->r3]);
            NEXT();
            
        L_MAX:
            regs[ip->r1] = std::max(regs[ip->r2], regs[ip->r3]);
            NEXT();
            
        L_EXT:
        {
            auto op_idx = static_cast<uint8_t>(ip->op) - static_cast<uint8_t>(OpCode::OP32);
            regs[ip->r1] = (regs[ip->r2] ^ op_idx) + regs[ip->r3];
            NEXT();
        }
        
        #undef DISPATCH
        #undef NEXT
        
        #else
        // Fallback to switch for non-GCC compilers
        return SwitchInterpreter::execute(code, state);
        #endif
    }
};

//==============================================================================
// 3. Indirect Threaded Code (ITC) - 指令中嵌入处理器地址
//    注意：标签地址必须在同一个函数内获取和使用
//==============================================================================

class IndirectThreadedInterpreter {
public:
    using Handler = void*;
    
    struct ITCInstruction {
        Handler handler;
        uint8_t r1, r2, r3;
        uint8_t op_idx;  // 用于扩展操作码
        int32_t imm;
    };
    
    // 合并compile和execute：先编译再执行
    [[gnu::noinline]]
    static uint64_t compile_and_execute(std::span<const Instruction> code, VMState& state) {
        #ifdef __GNUC__
        
        // 定义标签表（必须在使用的同一函数内）
        static constexpr void* handlers[] = {
            &&L_HALT, &&L_NOP,
            &&L_ADD, &&L_SUB, &&L_MUL,
            &&L_INC, &&L_DEC, &&L_NEG,
            &&L_AND, &&L_OR, &&L_XOR, &&L_NOT, &&L_SHL, &&L_SHR,
            &&L_LOAD_IMM, &&L_LOAD_REG, &&L_STORE,
            &&L_CMP_EQ, &&L_CMP_LT, &&L_CMP_GT,
            &&L_JMP, &&L_JZ, &&L_JNZ,
            &&L_MAC, &&L_DOT, &&L_POLY, &&L_ABS, &&L_MIN, &&L_MAX,
        };
        
        // 编译阶段
        std::vector<ITCInstruction> itc_code;
        itc_code.reserve(code.size());
        
        for (const auto& instr : code) {
            ITCInstruction itc;
            auto op_idx = static_cast<size_t>(instr.op);
            if (op_idx < NUM_BASIC_OPS) {
                itc.handler = handlers[op_idx];
            } else {
                itc.handler = &&L_EXT;
            }
            itc.r1 = instr.r1;
            itc.r2 = instr.r2;
            itc.r3 = instr.r3;
            itc.op_idx = (instr.op >= OpCode::OP32) ? 
                         static_cast<uint8_t>(instr.op) - static_cast<uint8_t>(OpCode::OP32) : 0;
            itc.imm = instr.imm;
            itc_code.push_back(itc);
        }
        
        // 执行阶段
        const ITCInstruction* ip = itc_code.data();
        const ITCInstruction* code_end = itc_code.data() + itc_code.size();
        auto& regs = state.regs;
        uint64_t flags = 0;
        
        #define DISPATCH() \
            do { \
                if (ip >= code_end) return regs[0]; \
                ++state.cycle_count; \
                goto *(ip->handler); \
            } while(0)
        
        #define NEXT() do { ++ip; DISPATCH(); } while(0)
        
        DISPATCH();
        
        L_HALT: return regs[0];
        L_NOP:  NEXT();
        L_ADD:  regs[ip->r1] = regs[ip->r2] + regs[ip->r3]; NEXT();
        L_SUB:  regs[ip->r1] = regs[ip->r2] - regs[ip->r3]; NEXT();
        L_MUL:  regs[ip->r1] = regs[ip->r2] * regs[ip->r3]; NEXT();
        L_INC:  ++regs[ip->r1]; NEXT();
        L_DEC:  --regs[ip->r1]; NEXT();
        L_NEG:  regs[ip->r1] = -regs[ip->r2]; NEXT();
        L_AND:  regs[ip->r1] = regs[ip->r2] & regs[ip->r3]; NEXT();
        L_OR:   regs[ip->r1] = regs[ip->r2] | regs[ip->r3]; NEXT();
        L_XOR:  regs[ip->r1] = regs[ip->r2] ^ regs[ip->r3]; NEXT();
        L_NOT:  regs[ip->r1] = ~regs[ip->r2]; NEXT();
        L_SHL:  regs[ip->r1] = regs[ip->r2] << (regs[ip->r3] & 63); NEXT();
        L_SHR:  regs[ip->r1] = static_cast<uint64_t>(regs[ip->r2]) >> (regs[ip->r3] & 63); NEXT();
        L_LOAD_IMM: regs[ip->r1] = ip->imm; NEXT();
        L_LOAD_REG: regs[ip->r1] = regs[ip->r2]; NEXT();
        L_STORE: regs[ip->r1] = regs[ip->r2]; NEXT();
        L_CMP_EQ: flags = (regs[ip->r2] == regs[ip->r3]) ? 1 : 0; NEXT();
        L_CMP_LT: flags = (regs[ip->r2] < regs[ip->r3]) ? 1 : 0; NEXT();
        L_CMP_GT: flags = (regs[ip->r2] > regs[ip->r3]) ? 1 : 0; NEXT();
        L_JMP:  ip = itc_code.data() + ip->imm; DISPATCH();
        L_JZ:   ip = (flags == 0) ? itc_code.data() + ip->imm : ip + 1; DISPATCH();
        L_JNZ:  ip = (flags != 0) ? itc_code.data() + ip->imm : ip + 1; DISPATCH();
        L_MAC:  regs[ip->r1] += regs[ip->r2] * regs[ip->r3]; NEXT();
        L_DOT:  regs[ip->r1] += regs[ip->r2] * regs[ip->r3]; NEXT();
        L_POLY: regs[ip->r1] = regs[ip->r1] * regs[ip->r2] + regs[ip->r3]; NEXT();
        L_ABS:  regs[ip->r1] = std::abs(regs[ip->r2]); NEXT();
        L_MIN:  regs[ip->r1] = std::min(regs[ip->r2], regs[ip->r3]); NEXT();
        L_MAX:  regs[ip->r1] = std::max(regs[ip->r2], regs[ip->r3]); NEXT();
        L_EXT:
        {
            regs[ip->r1] = (regs[ip->r2] ^ ip->op_idx) + regs[ip->r3];
            NEXT();
        }
        
        #undef DISPATCH
        #undef NEXT
        
        #else
        return SwitchInterpreter::execute(code, state);
        #endif
    }
    
    // 预编译版本 - 只执行（需要预编译好的代码）
    // 为了benchmark公平性，提供一个分离的执行版本
    [[gnu::noinline]]
    static uint64_t execute_precompiled(std::span<const ITCInstruction> itc_code, 
                                        VMState& state,
                                        void* const* handler_table) {
        #ifdef __GNUC__
        // 由于标签地址问题，这个方法实际上无法正确实现
        // 保留为fallback
        return 0;
        #else
        return 0;
        #endif
    }
};

//==============================================================================
// 4. Token Threaded Code (TTC) - 压缩的字节码
//==============================================================================

class TokenThreadedInterpreter {
public:
    // 紧凑的字节码格式
    struct CompactInstruction {
        uint8_t op;
        uint8_t r1_r2;    // r1: high 4 bits, r2: low 4 bits
        uint8_t r3;
        uint8_t padding;
        int32_t imm;
    };
    static_assert(sizeof(CompactInstruction) == 8);
    
    [[gnu::noinline]]
    static uint64_t execute(std::span<const CompactInstruction> code, VMState& state) {
        #ifdef __GNUC__
        
        static constexpr void* dispatch_table[] = {
            &&L_HALT, &&L_NOP,
            &&L_ADD, &&L_SUB, &&L_MUL,
            &&L_INC, &&L_DEC, &&L_NEG,
            &&L_AND, &&L_OR, &&L_XOR, &&L_NOT, &&L_SHL, &&L_SHR,
            &&L_LOAD_IMM, &&L_LOAD_REG, &&L_STORE,
            &&L_CMP_EQ, &&L_CMP_LT, &&L_CMP_GT,
            &&L_JMP, &&L_JZ, &&L_JNZ,
            &&L_MAC, &&L_DOT, &&L_POLY, &&L_ABS, &&L_MIN, &&L_MAX,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
            &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT, &&L_EXT,
        };
        
        const CompactInstruction* ip = code.data();
        const CompactInstruction* code_end = code.data() + code.size();
        auto& regs = state.regs;
        uint64_t flags = 0;
        
        // 解码寄存器操作数
        #define R1() (ip->r1_r2 >> 4)
        #define R2() (ip->r1_r2 & 0x0F)
        #define R3() (ip->r3)
        
        #define DISPATCH() \
            do { \
                if (ip >= code_end) return regs[0]; \
                ++state.cycle_count; \
                goto *dispatch_table[ip->op]; \
            } while(0)
        
        #define NEXT() do { ++ip; DISPATCH(); } while(0)
        
        DISPATCH();
        
        L_HALT: return regs[0];
        L_NOP:  NEXT();
        L_ADD:  regs[R1()] = regs[R2()] + regs[R3()]; NEXT();
        L_SUB:  regs[R1()] = regs[R2()] - regs[R3()]; NEXT();
        L_MUL:  regs[R1()] = regs[R2()] * regs[R3()]; NEXT();
        L_INC:  ++regs[R1()]; NEXT();
        L_DEC:  --regs[R1()]; NEXT();
        L_NEG:  regs[R1()] = -regs[R2()]; NEXT();
        L_AND:  regs[R1()] = regs[R2()] & regs[R3()]; NEXT();
        L_OR:   regs[R1()] = regs[R2()] | regs[R3()]; NEXT();
        L_XOR:  regs[R1()] = regs[R2()] ^ regs[R3()]; NEXT();
        L_NOT:  regs[R1()] = ~regs[R2()]; NEXT();
        L_SHL:  regs[R1()] = regs[R2()] << (regs[R3()] & 63); NEXT();
        L_SHR:  regs[R1()] = static_cast<uint64_t>(regs[R2()]) >> (regs[R3()] & 63); NEXT();
        L_LOAD_IMM: regs[R1()] = ip->imm; NEXT();
        L_LOAD_REG: regs[R1()] = regs[R2()]; NEXT();
        L_STORE: regs[R1()] = regs[R2()]; NEXT();
        L_CMP_EQ: flags = (regs[R2()] == regs[R3()]) ? 1 : 0; NEXT();
        L_CMP_LT: flags = (regs[R2()] < regs[R3()]) ? 1 : 0; NEXT();
        L_CMP_GT: flags = (regs[R2()] > regs[R3()]) ? 1 : 0; NEXT();
        L_JMP:  ip = code.data() + ip->imm; DISPATCH();
        L_JZ:   ip = (flags == 0) ? code.data() + ip->imm : ip + 1; DISPATCH();
        L_JNZ:  ip = (flags != 0) ? code.data() + ip->imm : ip + 1; DISPATCH();
        L_MAC:  regs[R1()] += regs[R2()] * regs[R3()]; NEXT();
        L_DOT:  regs[R1()] += regs[R2()] * regs[R3()]; NEXT();
        L_POLY: regs[R1()] = regs[R1()] * regs[R2()] + regs[R3()]; NEXT();
        L_ABS:  regs[R1()] = std::abs(regs[R2()]); NEXT();
        L_MIN:  regs[R1()] = std::min(regs[R2()], regs[R3()]); NEXT();
        L_MAX:  regs[R1()] = std::max(regs[R2()], regs[R3()]); NEXT();
        L_EXT:
        {
            regs[R1()] = (regs[R2()] ^ (ip->op - NUM_BASIC_OPS)) + regs[R3()];
            NEXT();
        }
        
        #undef R1
        #undef R2
        #undef R3
        #undef DISPATCH
        #undef NEXT
        
        #else
        return 0;
        #endif
    }
    
    static std::vector<CompactInstruction> compile(std::span<const Instruction> code) {
        std::vector<CompactInstruction> result;
        result.reserve(code.size());
        
        for (const auto& instr : code) {
            CompactInstruction ci;
            ci.op = static_cast<uint8_t>(instr.op);
            ci.r1_r2 = ((instr.r1 & 0x0F) << 4) | (instr.r2 & 0x0F);
            ci.r3 = instr.r3 & 0x0F;
            ci.padding = 0;
            ci.imm = instr.imm;
            result.push_back(ci);
        }
        return result;
    }
};

//==============================================================================
// 5. Subroutine Threaded Code (STC) - 函数指针调用
//==============================================================================

class SubroutineThreadedInterpreter {
public:
    using HandlerFn = bool(*)(VMState&, const Instruction&, const Instruction*, size_t);
    
    #define DEFINE_HANDLER(name, body) \
        static bool handle_##name(VMState& s, const Instruction& i, \
                                  const Instruction* base, size_t size) { \
            body; \
            return true; \
        }
    
    static bool handle_halt(VMState&, const Instruction&, const Instruction*, size_t) {
        return false;
    }
    
    DEFINE_HANDLER(nop, ++s.ip)
    DEFINE_HANDLER(add, s.regs[i.r1] = s.regs[i.r2] + s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(sub, s.regs[i.r1] = s.regs[i.r2] - s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(mul, s.regs[i.r1] = s.regs[i.r2] * s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(inc, ++s.regs[i.r1]; ++s.ip)
    DEFINE_HANDLER(dec, --s.regs[i.r1]; ++s.ip)
    DEFINE_HANDLER(neg, s.regs[i.r1] = -s.regs[i.r2]; ++s.ip)
    DEFINE_HANDLER(and_op, s.regs[i.r1] = s.regs[i.r2] & s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(or_op, s.regs[i.r1] = s.regs[i.r2] | s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(xor_op, s.regs[i.r1] = s.regs[i.r2] ^ s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(not_op, s.regs[i.r1] = ~s.regs[i.r2]; ++s.ip)
    DEFINE_HANDLER(shl, s.regs[i.r1] = s.regs[i.r2] << (s.regs[i.r3] & 63); ++s.ip)
    DEFINE_HANDLER(shr, s.regs[i.r1] = static_cast<uint64_t>(s.regs[i.r2]) >> (s.regs[i.r3] & 63); ++s.ip)
    DEFINE_HANDLER(load_imm, s.regs[i.r1] = i.imm; ++s.ip)
    DEFINE_HANDLER(load_reg, s.regs[i.r1] = s.regs[i.r2]; ++s.ip)
    DEFINE_HANDLER(store, s.regs[i.r1] = s.regs[i.r2]; ++s.ip)
    DEFINE_HANDLER(cmp_eq, s.flags = (s.regs[i.r2] == s.regs[i.r3]) ? 1 : 0; ++s.ip)
    DEFINE_HANDLER(cmp_lt, s.flags = (s.regs[i.r2] < s.regs[i.r3]) ? 1 : 0; ++s.ip)
    DEFINE_HANDLER(cmp_gt, s.flags = (s.regs[i.r2] > s.regs[i.r3]) ? 1 : 0; ++s.ip)
    DEFINE_HANDLER(jmp, s.ip = static_cast<size_t>(i.imm))
    DEFINE_HANDLER(jz, s.ip = (s.flags == 0) ? static_cast<size_t>(i.imm) : s.ip + 1)
    DEFINE_HANDLER(jnz, s.ip = (s.flags != 0) ? static_cast<size_t>(i.imm) : s.ip + 1)
    DEFINE_HANDLER(mac, s.regs[i.r1] += s.regs[i.r2] * s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(dot, s.regs[i.r1] += s.regs[i.r2] * s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(poly, s.regs[i.r1] = s.regs[i.r1] * s.regs[i.r2] + s.regs[i.r3]; ++s.ip)
    DEFINE_HANDLER(abs_op, s.regs[i.r1] = std::abs(s.regs[i.r2]); ++s.ip)
    DEFINE_HANDLER(min_op, s.regs[i.r1] = std::min(s.regs[i.r2], s.regs[i.r3]); ++s.ip)
    DEFINE_HANDLER(max_op, s.regs[i.r1] = std::max(s.regs[i.r2], s.regs[i.r3]); ++s.ip)
    
    static bool handle_ext(VMState& s, const Instruction& i, const Instruction*, size_t) {
        auto op_idx = static_cast<uint8_t>(i.op) - static_cast<uint8_t>(OpCode::OP32);
        s.regs[i.r1] = (s.regs[i.r2] ^ op_idx) + s.regs[i.r3];
        ++s.ip;
        return true;
    }
    
    #undef DEFINE_HANDLER
    
    [[gnu::noinline]]
    static uint64_t execute(std::span<const Instruction> code, VMState& state) {
        static constexpr HandlerFn handlers[] = {
            handle_halt, handle_nop,
            handle_add, handle_sub, handle_mul,
            handle_inc, handle_dec, handle_neg,
            handle_and_op, handle_or_op, handle_xor_op, handle_not_op, handle_shl, handle_shr,
            handle_load_imm, handle_load_reg, handle_store,
            handle_cmp_eq, handle_cmp_lt, handle_cmp_gt,
            handle_jmp, handle_jz, handle_jnz,
            handle_mac, handle_dot, handle_poly, handle_abs_op, handle_min_op, handle_max_op,
            // 扩展
            handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext,
            handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext,
            handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext,
            handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext, handle_ext,
        };
        
        const Instruction* base = code.data();
        const size_t size = code.size();
        
        while (state.ip < size) {
            ++state.cycle_count;
            const auto& instr = code[state.ip];
            auto op_idx = static_cast<size_t>(instr.op);
            if (op_idx >= std::size(handlers)) op_idx = NUM_BASIC_OPS;  // fallback to ext
            if (!handlers[op_idx](state, instr, base, size)) {
                break;
            }
        }
        return state.regs[0];
    }
};

//==============================================================================
// 6. Call Threaded Code (CTC) - 使用std::function / 内联函数表
//    这是一种更现代C++风格的实现
//==============================================================================

class CallThreadedInterpreter {
public:
    struct Context {
        VMState& state;
        const Instruction* code;
        size_t code_size;
        const Instruction* ip;
        uint64_t& flags;
    };
    
    using Handler = void(*)(Context&);
    
    #define CT_HANDLER(name, body) \
        static void h_##name(Context& ctx) { body; }
    
    static void h_halt(Context& ctx) { ctx.ip = ctx.code + ctx.code_size; }
    CT_HANDLER(nop, ++ctx.ip)
    CT_HANDLER(add, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] + ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(sub, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] - ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(mul, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] * ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(inc, ++ctx.state.regs[ctx.ip->r1]; ++ctx.ip)
    CT_HANDLER(dec, --ctx.state.regs[ctx.ip->r1]; ++ctx.ip)
    CT_HANDLER(neg, ctx.state.regs[ctx.ip->r1] = -ctx.state.regs[ctx.ip->r2]; ++ctx.ip)
    CT_HANDLER(and_op, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] & ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(or_op, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] | ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(xor_op, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] ^ ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(not_op, ctx.state.regs[ctx.ip->r1] = ~ctx.state.regs[ctx.ip->r2]; ++ctx.ip)
    CT_HANDLER(shl, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2] << (ctx.state.regs[ctx.ip->r3] & 63); ++ctx.ip)
    CT_HANDLER(shr, ctx.state.regs[ctx.ip->r1] = static_cast<uint64_t>(ctx.state.regs[ctx.ip->r2]) >> (ctx.state.regs[ctx.ip->r3] & 63); ++ctx.ip)
    CT_HANDLER(load_imm, ctx.state.regs[ctx.ip->r1] = ctx.ip->imm; ++ctx.ip)
    CT_HANDLER(load_reg, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2]; ++ctx.ip)
    CT_HANDLER(store, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r2]; ++ctx.ip)
    CT_HANDLER(cmp_eq, ctx.flags = (ctx.state.regs[ctx.ip->r2] == ctx.state.regs[ctx.ip->r3]) ? 1 : 0; ++ctx.ip)
    CT_HANDLER(cmp_lt, ctx.flags = (ctx.state.regs[ctx.ip->r2] < ctx.state.regs[ctx.ip->r3]) ? 1 : 0; ++ctx.ip)
    CT_HANDLER(cmp_gt, ctx.flags = (ctx.state.regs[ctx.ip->r2] > ctx.state.regs[ctx.ip->r3]) ? 1 : 0; ++ctx.ip)
    CT_HANDLER(jmp, ctx.ip = ctx.code + ctx.ip->imm)
    CT_HANDLER(jz, ctx.ip = (ctx.flags == 0) ? ctx.code + ctx.ip->imm : ctx.ip + 1)
    CT_HANDLER(jnz, ctx.ip = (ctx.flags != 0) ? ctx.code + ctx.ip->imm : ctx.ip + 1)
    CT_HANDLER(mac, ctx.state.regs[ctx.ip->r1] += ctx.state.regs[ctx.ip->r2] * ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(dot, ctx.state.regs[ctx.ip->r1] += ctx.state.regs[ctx.ip->r2] * ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(poly, ctx.state.regs[ctx.ip->r1] = ctx.state.regs[ctx.ip->r1] * ctx.state.regs[ctx.ip->r2] + ctx.state.regs[ctx.ip->r3]; ++ctx.ip)
    CT_HANDLER(abs_op, ctx.state.regs[ctx.ip->r1] = std::abs(ctx.state.regs[ctx.ip->r2]); ++ctx.ip)
    CT_HANDLER(min_op, ctx.state.regs[ctx.ip->r1] = std::min(ctx.state.regs[ctx.ip->r2], ctx.state.regs[ctx.ip->r3]); ++ctx.ip)
    CT_HANDLER(max_op, ctx.state.regs[ctx.ip->r1] = std::max(ctx.state.regs[ctx.ip->r2], ctx.state.regs[ctx.ip->r3]); ++ctx.ip)
    
    static void h_ext(Context& ctx) {
        auto op_idx = static_cast<uint8_t>(ctx.ip->op) - static_cast<uint8_t>(OpCode::OP32);
        ctx.state.regs[ctx.ip->r1] = (ctx.state.regs[ctx.ip->r2] ^ op_idx) + ctx.state.regs[ctx.ip->r3];
        ++ctx.ip;
    }
    
    #undef CT_HANDLER
    
    [[gnu::noinline]]
    static uint64_t execute(std::span<const Instruction> code, VMState& state) {
        static constexpr Handler handlers[] = {
            h_halt, h_nop,
            h_add, h_sub, h_mul,
            h_inc, h_dec, h_neg,
            h_and_op, h_or_op, h_xor_op, h_not_op, h_shl, h_shr,
            h_load_imm, h_load_reg, h_store,
            h_cmp_eq, h_cmp_lt, h_cmp_gt,
            h_jmp, h_jz, h_jnz,
            h_mac, h_dot, h_poly, h_abs_op, h_min_op, h_max_op,
            h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext,
            h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext,
            h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext,
            h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext, h_ext,
        };
        
        uint64_t flags = 0;
        Context ctx{state, code.data(), code.size(), code.data(), flags};
        
        while (ctx.ip < ctx.code + ctx.code_size) {
            ++state.cycle_count;
            auto op_idx = static_cast<size_t>(ctx.ip->op);
            if (op_idx >= std::size(handlers)) op_idx = NUM_BASIC_OPS;
            handlers[op_idx](ctx);
        }
        return state.regs[0];
    }
};

//==============================================================================
// 测试用例生成器
//==============================================================================

enum class Pattern {
    Sequential,    // 顺序遍历所有操作码
    Random,        // 完全随机
    Hotspot,       // 80%集中在少数几个操作
    Arithmetic,    // 主要是算术运算
    DataMovement,  // 主要是数据移动
    Mixed,         // 混合模式
    Loop,          // 包含循环
    BranchHeavy,   // 大量分支
};

class CodeGenerator {
public:
    CodeGenerator(size_t seed = 42) : rng_(seed) {}
    
    std::vector<Instruction> generate(size_t length, Pattern pattern) {
        std::vector<Instruction> code;
        code.reserve(length + 1);
        
        switch (pattern) {
            case Pattern::Sequential:   code = generateSequential(length); break;
            case Pattern::Random:       code = generateRandom(length); break;
            case Pattern::Hotspot:      code = generateHotspot(length); break;
            case Pattern::Arithmetic:   code = generateArithmetic(length); break;
            case Pattern::DataMovement: code = generateDataMovement(length); break;
            case Pattern::Mixed:        code = generateMixed(length); break;
            case Pattern::Loop:         code = generateLoop(length); break;
            case Pattern::BranchHeavy:  code = generateBranchHeavy(length); break;
        }
        
        // 添加HALT指令
        code.push_back({OpCode::HALT, 0, 0, 0, 0});
        return code;
    }
    
private:
    std::mt19937 rng_;
    
    uint8_t randReg() {
        return std::uniform_int_distribution<uint8_t>(0, NUM_REGS - 1)(rng_);
    }
    
    int32_t randImm() {
        return std::uniform_int_distribution<int32_t>(-1000, 1000)(rng_);
    }
    
    // 安全的操作码列表（不含跳转和HALT）
    static constexpr std::array<OpCode, 24> safe_ops = {
        OpCode::NOP, OpCode::ADD, OpCode::SUB, OpCode::MUL,
        OpCode::INC, OpCode::DEC, OpCode::NEG,
        OpCode::AND, OpCode::OR, OpCode::XOR, OpCode::NOT, OpCode::SHL, OpCode::SHR,
        OpCode::LOAD_IMM, OpCode::LOAD_REG, OpCode::STORE,
        OpCode::CMP_EQ, OpCode::CMP_LT, OpCode::CMP_GT,
        OpCode::MAC, OpCode::DOT, OpCode::POLY, OpCode::ABS, OpCode::MIN
    };
    
    std::vector<Instruction> generateSequential(size_t length) {
        std::vector<Instruction> code;
        for (size_t i = 0; i < length; ++i) {
            OpCode op = safe_ops[i % safe_ops.size()];
            code.push_back({op, randReg(), randReg(), randReg(), randImm()});
        }
        return code;
    }
    
    std::vector<Instruction> generateRandom(size_t length) {
        std::vector<Instruction> code;
        std::uniform_int_distribution<size_t> op_dist(0, safe_ops.size() - 1);
        
        for (size_t i = 0; i < length; ++i) {
            OpCode op = safe_ops[op_dist(rng_)];
            code.push_back({op, randReg(), randReg(), randReg(), randImm()});
        }
        return code;
    }
    
    std::vector<Instruction> generateHotspot(size_t length) {
        std::vector<Instruction> code;
        const std::array<OpCode, 4> hot_ops = {OpCode::ADD, OpCode::SUB, OpCode::MUL, OpCode::INC};
        const std::array<OpCode, 8> cold_ops = {
            OpCode::AND, OpCode::OR, OpCode::XOR, OpCode::SHL,
            OpCode::DEC, OpCode::NEG, OpCode::NOT, OpCode::MAC
        };
        
        std::uniform_real_distribution<double> prob(0, 1);
        std::uniform_int_distribution<size_t> hot_dist(0, hot_ops.size() - 1);
        std::uniform_int_distribution<size_t> cold_dist(0, cold_ops.size() - 1);
        
        for (size_t i = 0; i < length; ++i) {
            OpCode op = (prob(rng_) < 0.8) ? hot_ops[hot_dist(rng_)] : cold_ops[cold_dist(rng_)];
            code.push_back({op, randReg(), randReg(), randReg(), randImm()});
        }
        return code;
    }
    
    std::vector<Instruction> generateArithmetic(size_t length) {
        std::vector<Instruction> code;
        const std::array<OpCode, 8> arith_ops = {
            OpCode::ADD, OpCode::SUB, OpCode::MUL, OpCode::INC, OpCode::DEC,
            OpCode::NEG, OpCode::MAC, OpCode::POLY
        };
        std::uniform_int_distribution<size_t> op_dist(0, arith_ops.size() - 1);
        
        for (size_t i = 0; i < length; ++i) {
            code.push_back({arith_ops[op_dist(rng_)], randReg(), randReg(), randReg(), randImm()});
        }
        return code;
    }
    
    std::vector<Instruction> generateDataMovement(size_t length) {
        std::vector<Instruction> code;
        const std::array<OpCode, 5> data_ops = {
            OpCode::LOAD_IMM, OpCode::LOAD_REG, OpCode::STORE, OpCode::NOP, OpCode::INC
        };
        std::uniform_int_distribution<size_t> op_dist(0, data_ops.size() - 1);
        
        for (size_t i = 0; i < length; ++i) {
            code.push_back({data_ops[op_dist(rng_)], randReg(), randReg(), randReg(), randImm()});
        }
        return code;
    }
    
    std::vector<Instruction> generateMixed(size_t length) {
        return generateRandom(length);  // 和Random一样
    }
    
    std::vector<Instruction> generateLoop(size_t length) {
        std::vector<Instruction> code;
        
        // 初始化循环计数器
        int32_t iterations = static_cast<int32_t>(length / 10);
        if (iterations < 10) iterations = 10;
        
        code.push_back({OpCode::LOAD_IMM, 0, 0, 0, iterations});  // r0 = iterations
        code.push_back({OpCode::LOAD_IMM, 1, 0, 0, 0});           // r1 = 0 (比较用)
        code.push_back({OpCode::LOAD_IMM, 2, 0, 0, 0});           // r2 = 0 (累加器)
        
        size_t loop_start = code.size();
        
        // 循环体 - 简单操作
        for (int i = 0; i < 5; ++i) {
            code.push_back({OpCode::INC, 2, 0, 0, 0});
        }
        
        // 循环条件
        code.push_back({OpCode::DEC, 0, 0, 0, 0});
        code.push_back({OpCode::CMP_GT, 0, 0, 1, 0});  // flags = (r0 > r1)
        code.push_back({OpCode::JNZ, 0, 0, 0, static_cast<int32_t>(loop_start)});
        
        return code;
    }
    
    std::vector<Instruction> generateBranchHeavy(size_t length) {
        std::vector<Instruction> code;
        std::uniform_int_distribution<int32_t> imm_dist(-100, 100);
        
        for (size_t i = 0; i < length; ++i) {
            size_t base = code.size();
            code.push_back({OpCode::LOAD_IMM, 0, 0, 0, imm_dist(rng_)});
            code.push_back({OpCode::CMP_LT, 0, 0, 1, 0});
            // JZ跳过下一条指令或继续
            code.push_back({OpCode::JZ, 0, 0, 0, static_cast<int32_t>(base + 4)});
            code.push_back({OpCode::INC, 2, 0, 0, 0});
        }
        
        return code;
    }
};

//==============================================================================
// Benchmark 函数
//==============================================================================

// 通用benchmark模板
template<typename Interpreter>
void BM_Interpreter(benchmark::State& state, Pattern pattern) {
    const size_t code_length = static_cast<size_t>(state.range(0));
    CodeGenerator gen(12345);
    auto code = gen.generate(code_length, pattern);
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        
        auto result = Interpreter::execute(code, vm_state);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(vm_state);
    }
    
    state.SetItemsProcessed(state.iterations() * code_length);
    state.SetBytesProcessed(state.iterations() * code_length * sizeof(Instruction));
}

// ITC特殊处理（因为它合并了compile和execute）
void BM_ITC_Template(benchmark::State& state, Pattern pattern) {
    const size_t code_length = static_cast<size_t>(state.range(0));
    CodeGenerator gen(12345);
    auto code = gen.generate(code_length, pattern);
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        
        auto result = IndirectThreadedInterpreter::compile_and_execute(code, vm_state);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(vm_state);
    }
    
    state.SetItemsProcessed(state.iterations() * code_length);
}

// TTC特殊处理（预编译）
void BM_TTC_Template(benchmark::State& state, Pattern pattern) {
    const size_t code_length = static_cast<size_t>(state.range(0));
    CodeGenerator gen(12345);
    auto code = gen.generate(code_length, pattern);
    auto ttc_code = TokenThreadedInterpreter::compile(code);
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        
        auto result = TokenThreadedInterpreter::execute(ttc_code, vm_state);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(vm_state);
    }
    
    state.SetItemsProcessed(state.iterations() * code_length);
}

//==============================================================================
// 注册所有Benchmark
//==============================================================================

#define REGISTER_BENCHMARKS(Interpreter, prefix) \
    static void BM_##prefix##_Sequential(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::Sequential); \
    } \
    static void BM_##prefix##_Random(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::Random); \
    } \
    static void BM_##prefix##_Hotspot(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::Hotspot); \
    } \
    static void BM_##prefix##_Arithmetic(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::Arithmetic); \
    } \
    static void BM_##prefix##_DataMovement(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::DataMovement); \
    } \
    static void BM_##prefix##_Loop(benchmark::State& state) { \
        BM_Interpreter<Interpreter>(state, Pattern::Loop); \
    } \
    BENCHMARK(BM_##prefix##_Sequential)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000); \
    BENCHMARK(BM_##prefix##_Random)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000); \
    BENCHMARK(BM_##prefix##_Hotspot)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000); \
    BENCHMARK(BM_##prefix##_Arithmetic)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000); \
    BENCHMARK(BM_##prefix##_DataMovement)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000); \
    BENCHMARK(BM_##prefix##_Loop)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000);

// Switch-Case
REGISTER_BENCHMARKS(SwitchInterpreter, Switch)

// Direct Threaded Code
REGISTER_BENCHMARKS(DirectThreadedInterpreter, DTC)

// Subroutine Threaded Code
REGISTER_BENCHMARKS(SubroutineThreadedInterpreter, STC)

// Call Threaded Code
REGISTER_BENCHMARKS(CallThreadedInterpreter, CTC)

// ITC - 特殊处理
#define REGISTER_ITC_BENCHMARK(pattern_name, pattern_enum) \
    static void BM_ITC_##pattern_name(benchmark::State& state) { \
        BM_ITC_Template(state, Pattern::pattern_enum); \
    } \
    BENCHMARK(BM_ITC_##pattern_name)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000);

REGISTER_ITC_BENCHMARK(Sequential, Sequential)
REGISTER_ITC_BENCHMARK(Random, Random)
REGISTER_ITC_BENCHMARK(Hotspot, Hotspot)
REGISTER_ITC_BENCHMARK(Arithmetic, Arithmetic)
REGISTER_ITC_BENCHMARK(Loop, Loop)

// TTC - 特殊处理
#define REGISTER_TTC_BENCHMARK(pattern_name, pattern_enum) \
    static void BM_TTC_##pattern_name(benchmark::State& state) { \
        BM_TTC_Template(state, Pattern::pattern_enum); \
    } \
    BENCHMARK(BM_TTC_##pattern_name)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000);

REGISTER_TTC_BENCHMARK(Sequential, Sequential)
REGISTER_TTC_BENCHMARK(Random, Random)
REGISTER_TTC_BENCHMARK(Hotspot, Hotspot)
REGISTER_TTC_BENCHMARK(Arithmetic, Arithmetic)
REGISTER_TTC_BENCHMARK(Loop, Loop)

//==============================================================================
// 特殊场景测试
//==============================================================================

// 紧密循环测试 - Switch
static void BM_Switch_TightLoop(benchmark::State& state) {
    std::vector<Instruction> code = {
        {OpCode::LOAD_IMM, 0, 0, 0, 100000},   // 循环次数
        {OpCode::LOAD_IMM, 1, 0, 0, 0},        // 比较值
        {OpCode::INC, 2, 0, 0, 0},             // 循环开始
        {OpCode::INC, 3, 0, 0, 0},
        {OpCode::DEC, 0, 0, 0, 0},
        {OpCode::CMP_GT, 0, 0, 1, 0},
        {OpCode::JNZ, 0, 0, 0, 2},             // 跳回循环
        {OpCode::HALT, 0, 0, 0, 0},
    };
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        auto result = SwitchInterpreter::execute(code, vm_state);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 100000 * 5);  // 5条指令/迭代
}
BENCHMARK(BM_Switch_TightLoop);

// 紧密循环测试 - DTC
static void BM_DTC_TightLoop(benchmark::State& state) {
    std::vector<Instruction> code = {
        {OpCode::LOAD_IMM, 0, 0, 0, 100000},
        {OpCode::LOAD_IMM, 1, 0, 0, 0},
        {OpCode::INC, 2, 0, 0, 0},
        {OpCode::INC, 3, 0, 0, 0},
        {OpCode::DEC, 0, 0, 0, 0},
        {OpCode::CMP_GT, 0, 0, 1, 0},
        {OpCode::JNZ, 0, 0, 0, 2},
        {OpCode::HALT, 0, 0, 0, 0},
    };
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        auto result = DirectThreadedInterpreter::execute(code, vm_state);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 100000 * 5);
}
BENCHMARK(BM_DTC_TightLoop);

// 大量不同操作码测试
static void BM_Switch_ManyOpcodes(benchmark::State& state) {
    std::vector<Instruction> code;
    std::mt19937 rng(42);
    
    // 使用包括扩展操作码在内的所有操作
    for (int i = 0; i < 10000; ++i) {
        OpCode op;
        int op_val = rng() % 60;
        if (op_val >= static_cast<int>(OpCode::JMP) && op_val <= static_cast<int>(OpCode::JNZ)) {
            op = OpCode::ADD;  // 替换跳转
        } else if (op_val == 0) {
            op = OpCode::NOP;  // 替换HALT
        } else {
            op = static_cast<OpCode>(op_val);
        }
        code.push_back({op, 
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<int32_t>(rng() % 1000)});
    }
    code.push_back({OpCode::HALT, 0, 0, 0, 0});
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        auto result = SwitchInterpreter::execute(code, vm_state);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 10000);
}
BENCHMARK(BM_Switch_ManyOpcodes);

static void BM_DTC_ManyOpcodes(benchmark::State& state) {
    std::vector<Instruction> code;
    std::mt19937 rng(42);
    
    for (int i = 0; i < 10000; ++i) {
        OpCode op;
        int op_val = rng() % 60;
        if (op_val >= static_cast<int>(OpCode::JMP) && op_val <= static_cast<int>(OpCode::JNZ)) {
            op = OpCode::ADD;
        } else if (op_val == 0) {
            op = OpCode::NOP;
        } else {
            op = static_cast<OpCode>(op_val);
        }
        code.push_back({op,
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<uint8_t>(rng() % 16),
                       static_cast<int32_t>(rng() % 1000)});
    }
    code.push_back({OpCode::HALT, 0, 0, 0, 0});
    
    for (auto _ : state) {
        VMState vm_state;
        vm_state.reset();
        auto result = DirectThreadedInterpreter::execute(code, vm_state);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * 10000);
}
BENCHMARK(BM_DTC_ManyOpcodes);

BENCHMARK_MAIN();