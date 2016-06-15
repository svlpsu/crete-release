#ifndef RUNTIME_DUMP_H
#define RUNTIME_DUMP_H

#include <inttypes.h>

#include "config-target.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TCGContext;
struct TranslationBlock;
struct RuntimeEnv;

enum MemoMergePoint_ty{
	NormalTb = 0,
	BackToInterestTb = 1,
	OutofInterestTb = 2,
	//special case for the tb which is not only the first tb from disinterested tb
	//to interested tb, but also the last tb from interest tb to disinterested tb
	OutAndBackTb = 3
};

extern struct RuntimeEnv *runtime_env;

extern struct TranslationBlock *rt_dump_tb;
extern uint64_t rt_dump_tb_count;

extern int flag_rt_dump_start;
extern int flag_rt_dump_enable;
extern int flag_getHostAddress;
extern int flag_interested_tb;
extern int flag_interested_tb_prev;
extern int flag_memo_monitor_enable;

extern int crete_flag_capture_enabled; // Enabled/Disabled on capture_begin/end. Can be disabled on command (e.g., crete_debug_capture()).

#if defined(CRETE_DBG_CALL_STACK)
extern int flag_is_first_iteration;
extern int flag_enable_monitor_call_stack;
extern int flag_holdon_monitor_call_stack;

extern int is_begin_capture;
extern int is_target_pid;
extern int is_user_code;

extern uint64_t addr_main_function;
extern uint64_t size_main_function;
extern bool call_stack_started;

extern uint64_t g_crete_target_pid;
extern int g_custom_inst_emit;

#if defined(TARGET_X86_64)
    #define USER_CODE_RANGE 0x00007FFFFFFFFFFF
#elif defined(TARGET_I386)
    #define USER_CODE_RANGE 0xC0000000 // TODO: Should technically be 0xBFFFFFFF
#else
    #error CRETE: Only I386 and x64 supported!
#endif // defined(TARGET_X86_64) || defined(TARGET_I386)

extern uint32_t g_crete_call_stack_bound;
#endif

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
extern int flag_dump_interrupt_CPUState;
#endif
/*****************************/
/* Functions for QEMU c code */
void initialize_std_output(void);

struct RuntimeEnv* runtime_dump_initialize(void);
void runtime_dump_close(struct RuntimeEnv *rt);

void crete_runtime_dump(void *dumpCpuState, TranslationBlock *tb);
int crete_post_runtime_dump(void *qemuCpuState, TranslationBlock *tb);

void dump_CPUState(struct RuntimeEnv *rt, void *dumpCpuState);
void dump_ConcolicData(struct RuntimeEnv *rt, void *dumpCpuState);

void dump_prolog_regs(struct RuntimeEnv *rt, void *env_CpuState, int is_valid);
void dump_TBExecSequ(struct RuntimeEnv *rt, TranslationBlock *tb);
void dump_printInfo(struct RuntimeEnv *rt);
void dump_writeRtEnvToFile(struct RuntimeEnv *rt, const char *outputDirectory);

void add_memo_sync_table(struct RuntimeEnv *rtm);
void dump_memo_sync_table_entry(struct RuntimeEnv *rt, uint64_t addr, uint32_t size, uint64_t value);
void add_memo_merge_point(struct RuntimeEnv *rt, enum MemoMergePoint_ty type_MMP);

#if defined(CRETE_DBG_CALL_STACK)
void runtime_call_stack_update(struct RuntimeEnv *rt, const TranslationBlock *tb);
int  runtime_call_stack_size(struct RuntimeEnv *rt);

void push_interrupt_stack(struct RuntimeEnv *rt, int intno, int is_int,
		int error_code, int next_eip, uint64_t interrupted_pc);

void pop_interrupt_stack(struct RuntimeEnv *rt, uint64_t resumed_pc);

#endif

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
void add_qemu_interrupt_state(struct RuntimeEnv *rt,
		int intno, int is_int, int error_code, int next_eip_addend);
void add_empty_qemu_interrupt_state(struct RuntimeEnv *rt);
void dump_cpuState_for_interrupt(struct RuntimeEnv *rt,void *dumpCpuState);

#endif

#if defined(CRETE_UNUSED_CODE) && 0
void dump_TLBTable(struct RuntimeEnv *rt, void *env_CpuState, int is_pf);
void dump_prolog_TLBTable(struct RuntimeEnv *rt, void *env_CpuState, int is_valid);
void dump_Esp(struct RuntimeEnv *rt, void *env_CpuState);
void dump_Ebp(struct RuntimeEnv *rt, void *env_CpuState);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <vector>
#include <set>
#include <map>
#include <stack>
#include <sstream>

/***********************************/
/* External interface for C++ code */
#include <llvm/Support/raw_ostream.h>

using namespace std;

struct ConcreteMemoInfo
{
	uint64_t m_addr;
	uint32_t m_size;
	vector<uint8_t> m_data;

	ConcreteMemoInfo(uint64_t addr, uint32_t size, vector<uint8_t> data)
	:m_addr(addr), m_size(size), m_data(data) {}
};

// addr is the key for this map
typedef map<uint64_t, ConcreteMemoInfo> memoSyncTable_ty;
typedef vector<memoSyncTable_ty> memoSyncTables_ty;

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
struct QemuInterruptInfo {
	int m_intno;
	int m_is_int;
	int m_error_code;
	int m_next_eip_addend;

	QemuInterruptInfo(int intno, int is_int, int error_code, int next_eip_addend)
	:m_intno(intno), m_is_int(is_int),
	 m_error_code(error_code), m_next_eip_addend(next_eip_addend) {}
};

// The data type that will be used for replaying the interrupt/exception
// pair::first is the information of qemu interrupt
// pair::second is the CPUState when this interrupt completes
typedef pair<QemuInterruptInfo, void *> interruptState_ty;
#endif //#if defined(CRETE_DBG_REPLAY_INTERRUPT)

#if defined(CRETE_DBG_CALL_STACK)
struct CallStackEntry
{
	uint64_t m_start_addr;
	uint64_t m_size;
	string   m_func_name;
	CallStackEntry(uint64_t addr, uint32_t size, string func_name)
	:m_start_addr(addr), m_size(size), m_func_name(func_name) {}
};

enum TBCallStackType
{
	CST_INVALID = 0,   // Invalid Type
	CST_FUNC_CALL = 1, // This is the first TB after a function call instruction
	CST_FUNC_RET = 2,  // This is the first TB after a function ret instruction
	CST_FUNC_CONT = 3 // This is a TB that stays the same function as the previous TB
};

extern map<uint64_t, CallStackEntry> elf_symtab_functions;

// The data type that will be used for interrupt monitor
// pair::first is the information of qemu interrupt
// pair::second is the pc value of the tb that was interrupted
typedef pair<QemuInterruptInfo, uint64_t> InterruptInfoWithPC_ty;
#endif

pair<uint64_t, vector<uint8_t> > guest_read_buf(uint64_t addr, uint64_t size, void* env_cpuState);

#include "tcg-llvm-offline/tcg-llvm-offline.h"

class RuntimeEnv
{
public:
    struct ConcolicMemoryObject
    {
        ConcolicMemoryObject(string name,
                             uint64_t name_addr,
                             uint64_t data_guest_addr,
                             uint64_t data_host_addr,
                             uint64_t data_size) :
            name_(name),
            name_addr_(name_addr),
            data_guest_addr_(data_guest_addr),
            data_host_addr_(data_host_addr),
            data_size_(data_size) {}
        string name_;
        uint64_t name_addr_, data_guest_addr_, data_host_addr_, data_size_;
    };

private:
	// CPUStates saved from run-time
	vector<void *> m_cpuStates;

	// Dumped before the execution of every interested TB
	vector<void *> m_prolog_regs;

	// Each entry stores all load memory operations for each unique addr for each interested TB
	// Each entry is a memoSyncTable, disinterested TB will have an empty table
	memoSyncTables_ty m_memoSyncTables;
	vector<MemoMergePoint_ty> m_memoMergePoints;

	// dumped memories, format: "name value size, guestAddress:hostAddress\n"
	vector<string> m_symbMemos;

    vector<ConcolicMemoryObject> m_makeConcolics; // TODO: Should be a set, or unordered_set (boost), for fast lookup, nonredudance

	// Execution sequence in terms of Translation Block
    vector<string> m_tbExecSequ;
	set<string> m_tbDecl;

    string m_outputDirectory;

#if defined(CRETE_DBG_CALL_STACK)
    stack<CallStackEntry> m_callStack;
    // The type of the current executing TB for call stack
    TBCallStackType m_tb_cst;
    // The stack of interrupt happened in the execution of the target program
    // stack is not empty indicates, the program is under interrupt processing
    stack<InterruptInfoWithPC_ty> m_interruptStack;
#endif

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
    // The interrupts and their state information collected during the execution of the program
    // (in raise_interrupt()), and hence will only contains interrupts invoked
    // by the program (the interrupt raised outsided the program will not be captured, such as
    // page fault when loading code, hardware interrupt from keyboard, timer, etc)
    vector< interruptState_ty > m_interruptStates;
#endif

#if defined(CRETE_DBG_TB_GRAPH)
    vector<uint64_t> m_tbExecSequInt;
#endif // defined(CRETE_DBG_TB_GRAPH)

    TCGLLVMOfflineContext m_tcg_llvm_offline_ctx;
public:
    enum DumpMemoType {
    	ConcreteMemo, SymbolicMemo
    };

	RuntimeEnv();
	~RuntimeEnv();

	void addCpuStates(void *dumpCpuState);

	void addPrologRegs(void *env_cpuState, int is_valid);

	void addMemoSyncTable();
	void addMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value);
	void addMemoMergePoint(MemoMergePoint_ty type_MMP);

	void addTBExecSequ(TranslationBlock *tb);

	void addMemoStr(string str_memo,
			DumpMemoType memo_type = ConcreteMemo);

    void addConcolicData(ConcolicMemoryObject& cmo);

	void writeRtEnvToFile(const string& outputDirectory);

	void printInfo();

    static void readCpuRegister(void *env_cpuState, void *buf,
    		uint32_t offset, uint32_t size);
    static uint64_t getHostAddress(void *env_cpuState,
    		uint64_t guest_virtual_addr, int mmu_idx, int is_write = 1);

    void dumpConcolicData();
    void feed_test_case(const string& file_name);
    void dump_initial_input();

    void initOutputDirectory(const string& outputDirectory);

    void reverseTBDump(void *qemuCpuState, TranslationBlock *tb);
#if defined(CRETE_DBG_CALL_STACK)
    void updateCallStack(const TranslationBlock *tb);
    void prepare_call_stack_update(const TranslationBlock *tb);
    int  getCallStackSize() const;

    void finishCallStack();

    void pushInterruptStack(QemuInterruptInfo interrup_info, uint64_t interrupted_pc);
    void popInterruptStack(uint64_t resumed_pc);
    bool isProcessingInterrupt();
#endif

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
    void addQemuInterruptState(QemuInterruptInfo interrup_info);
    void addEmptyQemuInterruptState();
    void dumpCpuStateForInterrupt(void *dumpCpuState);

    void verifyDumpData();
#endif

    void dump_tlo_tb_pc(const uint64_t pc);

    void dump_tcg_ctx(const TCGContext& tcg_ctx);
    void dump_tcg_temp(const vector<TCGTemp>& tcg_temp);
    void dump_tcg_helper_name(const TCGContext &tcg_ctx);

    void dump_tlo_opc_buf(const uint64_t *opc_buf);
    void dump_tlo_opparam_buf(const uint64_t *opparam_buf);

#if defined(CRETE_DBG_TB_GRAPH)
    void addTBGraphInfo(TranslationBlock *tb);
#endif // defined(CRETE_DBG_TB_GRAPH)

private:
	string getOutputFilename(const string &fileName);
	llvm::raw_ostream* openOutputFile(const string &fileName);
	void writeDebugToFile();
    void writeLlvmMainFunction();

    void writeSymbolicMemo();

    void writePrologRegs();

    bool overlaps_with_existing_mo(uint64_t addr, size_t size);

	void mergeMemoSyncTables();
	void writeMemoSyncTables();

	vector<uint64_t> overlapsMemoSyncEntry(uint64_t addr,
			uint32_t size, memoSyncTable_ty target_memoSyncTable);

	void addMemoSyncTableEntryInternal(uint64_t addr, uint32_t size, vector<uint8_t> v_value,
			 memoSyncTable_ty& target_memoSyncTable);

	void verifyMemoSyncTable(const memoSyncTable_ty& target_memoSyncTable);
	void debugMergeMemoSync();
	void print_memoSyncTables();

#if defined(CRETE_DBG_CALL_STACK)
    void print_callStack();
    void init_inst_based_call_stack();
#endif

#if defined(CRETE_DBG_REPLAY_INTERRUPT)
    void writeInterruptStates();
#endif

	void writeTcgLlvmCtx();

#if defined(CRETE_DBG_TB_GRAPH)
    void writeTBAddresses();
    void writeNewTraceLog();
#endif // defined(CRETE_DBG_TB_GRAPH)

#if defined(CRETE_UNUSED_CODE) && 0
public:

    struct ElfMemo
    {
        uint64_t addr, size;
        vector<uint8_t> data;
    };

private:
	vector<uint64_t> m_esps;
	vector<uint64_t> m_ebps;

	vector<void *> m_prolog_tlbTables; //TLB tables will be used for prolog
    vector<void *> m_tlbTables;		// TLB tables will be used for tlb_fill

    vector<string> m_concMemos;

    // <addr, size>
    pair<uint64_t, uint64_t> m_rodataInfo;

    vector<ElfMemo> elf_memos;

public:
	void addTlbTable(void *env_cpuState, int is_pf);

	void addEsp(void *env_cpuState);
	void addEbp(void *env_cpuState);

	void addPrologTlbTable(void *env_cpuState, int is_valid);

	void setRodataInfo(uint64_t addr, uint64_t size);

	void addElfMemo(const ElfMemo& elf_memo);

private:
    void writeConcreteMemo();
    void writeTbPrologue();

    void writeTlbTables();
    void writePrologTlbTables();

    void addEspMemos(void *env_cpuState, uint32_t size = -1, uint32_t offset = 0);
	void dumpRodata(void *env_cpuState);
    void dumpElfMemos();
#endif //#if defined(CRETE_UNUSED_CODE) && 0
};


#endif  /* __cplusplus end*/

#endif  /* RUNTIME_DUMP_H end */
