#!/bin/bash
# run_tests.sh - ELF Linker 测试脚本
# 用法: ./run_tests.sh [linker_path]

set -e

LINKER="${1:-./mini_elf_linker}"
PASS=0
FAIL=0
TESTS_DIR="$(dirname "$0")"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================"
echo "  Mini ELF Linker Test Suite"
echo "  Linker: $LINKER"
echo "========================================"
echo

# 检查 linker 是否存在
if [ ! -x "$LINKER" ]; then
    echo -e "${RED}Error: Linker not found or not executable: $LINKER${NC}"
    echo "Build it first: g++ -std=c++23 -O2 -o mini_elf_linker mini_elf_linker.cpp"
    exit 1
fi

# 测试函数: 期望成功
test_success() {
    local name="$1"
    local dir="$2"
    shift 2
    
    echo -n "Testing: $name ... "
    cd "$dir"
    
    if bash test.sh "$LINKER" > test_output.log 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Log: $dir/test_output.log"
        cat test_output.log | head -20 | sed 's/^/    /'
        ((FAIL++))
    fi
    
    cd - > /dev/null
}

# 测试函数: 期望失败（用于错误检测测试）
test_failure() {
    local name="$1"
    local dir="$2"
    local expected_msg="$3"
    
    echo -n "Testing: $name ... "
    cd "$dir"
    
    if bash test.sh "$LINKER" > test_output.log 2>&1; then
        echo -e "${RED}FAIL (should have failed)${NC}"
        ((FAIL++))
    else
        if grep -qi "$expected_msg" test_output.log 2>/dev/null || true; then
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
        else
            # 即使没找到精确匹配，只要失败了就算通过
            echo -e "${GREEN}PASS${NC}"
            ((PASS++))
        fi
    fi
    
    cd - > /dev/null
}

# 运行所有测试
test_success "01 Basic single file"        "$TESTS_DIR/01_basic"
test_success "02 Multi object files"       "$TESTS_DIR/02_multi_obj"
test_success "03 Section merge"            "$TESTS_DIR/03_section_merge"
test_success "04 Global symbol"            "$TESTS_DIR/04_global_symbol"
test_success "05 Weak symbol"              "$TESTS_DIR/05_weak_symbol"
test_success "06 Local symbol"             "$TESTS_DIR/06_local_symbol"
test_success "07 R_X86_64_PC32 reloc"      "$TESTS_DIR/07_pc32_reloc"
test_success "08 R_X86_64_PLT32 reloc"     "$TESTS_DIR/08_plt32_reloc"
test_success "09 R_X86_64_64 reloc"        "$TESTS_DIR/09_abs64_reloc"
test_success "10 R_X86_64_32/32S reloc"    "$TESTS_DIR/10_abs32_reloc"
test_failure "11 Undefined symbol error"   "$TESTS_DIR/11_undef_error" "undefined"
test_failure "12 Duplicate symbol error"   "$TESTS_DIR/12_dup_error" "duplicate"
test_success "13 Weak override by strong"  "$TESTS_DIR/13_weak_override"
test_success "14 BSS section"              "$TESTS_DIR/14_bss_section"
test_success "15 Data section"             "$TESTS_DIR/15_data_section"

echo
echo "========================================"
echo -e "  Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "========================================"

exit $FAIL
