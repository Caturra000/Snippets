// file: ipointer_tester.cpp
#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <algorithm>

std::ofstream OutFile;

KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "pintool-IPOINT_AFTER.out", "specify output file name");

const std::unordered_set<std::string> target_rtn_names {
    "main",
    "_Z14dummy_functionv",
    "_Z13instrument_mei",
};

/* ===================================================================== */
// Analysis Routines
/* ===================================================================== */

void disassemble_showonce(const std::string *disassemble) {
    OutFile << std::left << std::setw(36) << disassemble->c_str() << "\t";
}

VOID BeforeCall(ADDRINT ip, const std::string *disassemble) {
    disassemble_showonce(disassemble);
    OutFile << "[PIN]" << std::endl;
}

VOID AfterCall(ADDRINT ip, const std::string *disassemble) {
    disassemble_showonce(disassemble);
    OutFile << "[PIN]" << "[AFTER]" << std::endl;
    // OutFile << "[PIN] " << std::hex << ip << ": [AFTER CALL]" << std::endl;
}

VOID TakenBranch(ADDRINT ip, const std::string *disassemble/*, ADDRINT target*/) {
    disassemble_showonce(disassemble);
    OutFile << "[PIN]" << "[TAKEN BRANCH]" << std::endl;
    //OutFile << "[PIN] " << std::hex << ip << ": [TAKEN BRANCH] Jumping to " << std::hex << target << std::endl;
}

/* ===================================================================== */
// Instrumentation Routine
/* ===================================================================== */

void Instruction(INS ins, VOID *v) {
    std::string disassemble = INS_Disassemble(ins);
    auto insert = [&](auto where, auto callback) {
        INS_InsertCall(ins, where, (AFUNPTR) callback,
                        IARG_INST_PTR,
                        // 官方 trace.cpp 使用的方法，难受也没办法，简单测试不管回收了
                        IARG_PTR, new std::string(disassemble),
                        IARG_END);
    };

    // before 永远合法
    insert(IPOINT_BEFORE, BeforeCall);

    if(INS_IsValidForIpointAfter(ins)) {
        insert(IPOINT_AFTER, AfterCall);
    }

    if(INS_IsValidForIpointTakenBranch(ins)) {
        insert(IPOINT_TAKEN_BRANCH, TakenBranch);
    }
}

VOID Routine(RTN rtn, VOID *v) {
    if(target_rtn_names.count(RTN_Name(rtn)) == 0) return;
    OutFile << "Found and instrumenting function: " << RTN_Name(rtn) << std::endl;

    RTN_Open(rtn);

    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        Instruction(ins, 0);
    }

    RTN_Close(rtn);
}

VOID Fini(INT32 code, VOID *v) {
    OutFile.close();
}

// -case 1 对应于 test1
// -case 2 对应于 test2 (WIP!)
KNOB<std::string> case_selector(KNOB_MODE_WRITEONCE, "pintool", "case", "1", "select your study case. Currently 1 or 2.");

int main(int argc, char * argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return -1;

    OutFile.open(KnobOutputFile.Value().c_str());
    OutFile << std::hex;

    auto icase = case_selector.Value();
    if(icase == "1") {
        OutFile << "Start case 1." << std::endl;
        RTN_AddInstrumentFunction(Routine, 0);
    }
    // TODO：有些问题，后面再改
    if(icase == "2") {
        OutFile << "Start case 2." << std::endl;
        INS_AddInstrumentFunction(Instruction, 0);
    }
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}
