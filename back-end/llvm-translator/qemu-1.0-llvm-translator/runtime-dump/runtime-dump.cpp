#include "runtime-dump.h"

#include <boost/serialization/split_member.hpp>
#include <string>
#include <stdlib.h>

extern "C" {
#include "tcg.h"
#include "cpu.h"
#include "exec-all.h"
#include "config-target.h"
}

#include <tcg/tcg-llvm.h>

#include <llvm/Value.h>
#include <llvm/Function.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_os_ostream.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>

#include <crete/test_case.h>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#if defined(HAVLICEK_TB_GRAPH) || 1
#include <stdexcept>
#include <boost/filesystem/operations.hpp>
#endif // defined(HAVLICEK_TB_GRAPH) || 1

#include "custom-instructions.h"

using namespace std;

extern "C" {
RuntimeEnv *runtime_env = 0;

TranslationBlock *rt_dump_tb = 0;
/* count how many tb that is of insterest has been executed including the current one*/
uint64_t rt_dump_tb_count = 0;

/* flag for runtime dump start: 0 = disable, 1 = enable*/
int flag_rt_dump_start = 0;
/* flag for runtime dump: 0 = disable, 1 = enable*/
int	flag_rt_dump_enable = 0;
/* flag to indicate whether it is during the execution of RuntimeEnv::getHostAddress()*/
int flag_getHostAddress = 0;
/* flag to indicate whether the current tb is of interest*/
int flag_interested_tb = 0;
/* flag to indicate whether the previous tb is of interest*/
int flag_interested_tb_prev = 0;
/* flag to indicate whether the memory monitoring is enabled.
 * This flag will be enabled when the execution of the program comes back
 * from disinterested tb to interested tb*/
int flag_memo_monitor_enable = 0;

#if defined(DBG_BO_CALL_STACK)
uint32_t g_crete_call_stack_bound = 0;

/* flag to indicate whether this is in the first iteration of crete workflow */
int flag_is_first_iteration = 0;
/* flag to indicate whether monitoring call stack is enabled or not.
 * In every iteration of crete workflow, it will be enabled initially
 * and will be disabled when the base function of call stack returns*/
int flag_enable_monitor_call_stack = 1;
/* flag to indicate whether monitoring call stack is paused or not.
 * If this flag is enabled, check on the update of call stack will continue,
 * but the call stack will not be updated.*/
int flag_holdon_monitor_call_stack = 0;

int is_begin_capture = 0;
int is_target_pid = 0;
int is_user_code = 0;

uint64_t addr_main_function = 0;
uint64_t size_main_function = 0;

bool call_stack_started = false;
#endif //#if defined(DBG_BO_CALL_STACK)

#if defined(DBG_BO_REPLAY_INTERRUPT)
int flag_dump_interrupt_CPUState = 0;
#endif

uint64_t g_havlicek_target_pid = 0;
int g_custom_inst_emit = 0;
int crete_flag_capture_enabled = 0;

extern int crete_is_include_filter_empty(void);
extern int crete_is_pc_in_exclude_filter_range(uint64_t pc); // defined in custom-instructions.cpp
extern int crete_is_pc_in_include_filter_range(uint64_t pc); // defined in custom-instructions.cpp
extern int crete_is_pc_in_call_stack_exclude_filter_range(uint64_t pc); // defined in custom-instructions.cpp

extern void dump_IR(void *, unsigned long long);

/* global variable of CPUState, which will be used in custom instruction handler.*/
CPUState *g_cpuState_bct = 0;
}

CPUState;
#define CPU_OFFSET(field) offsetof(CPUState, field)

#if defined(DBG_BO_CALL_STACK)
/* The collection of all the functions in symbol table of elf files.
 * The key of this map is the starting address of each function*/
map<uint64_t, CallStackEntry> elf_symtab_functions;
#endif

#if defined(HAVLICEK_TB_GRAPH)
stack<bool> crete_last_brcond_result;
#endif // defined(HAVLICEK_TB_GRAPH)

#if defined(DBG_BO_TODO) || 1
#define DATA_SIZE (1 << SHIFT)
#endif
/***********************************/
/* External interface for C++ code */
RuntimeEnv::RuntimeEnv()
{
}

RuntimeEnv::~RuntimeEnv()
{
	while(!m_cpuStates.empty()) {
		uint8_t *ptr_cpuState = (uint8_t *) m_cpuStates.back();
		m_cpuStates.pop_back();
        delete [] ptr_cpuState;
	}

	while(!m_prolog_regs.empty()) {
		uint8_t *ptr_temp = (uint8_t *) m_prolog_regs.back();
		m_prolog_regs.pop_back();
        delete [] ptr_temp;
	}

	while(!m_interruptStates.empty()) {
		uint8_t *ptr_temp = (uint8_t *) m_interruptStates.back().second;
		m_interruptStates.pop_back();
        delete [] ptr_temp;
	}

#if defined(CRETE_UNUSED_CODE) && 0
	while(!m_tlbTables.empty()) {
		uint8_t *ptr_tlbTable = (uint8_t *) m_tlbTables.back();
		m_tlbTables.pop_back();
		delete ptr_tlbTable;
	}

	while(!m_prolog_tlbTables.empty()) {
		uint8_t *ptr_tlbTable = (uint8_t *) m_prolog_tlbTables.back();
		m_prolog_tlbTables.pop_back();
		delete ptr_tlbTable;
	}
#endif
}

// Add dumpCpuState to cpuStates
void RuntimeEnv::addCpuStates(void *dumpCpuState)
{
	CPUState *src_cpuState = (CPUState *) dumpCpuState;
	assert(src_cpuState);

	CPUState *dst_cpuState = (CPUState *) new uint8_t [sizeof(CPUState)];
	memcpy(dst_cpuState, src_cpuState, sizeof(CPUState));

	m_cpuStates.push_back((void *) dst_cpuState);

    // Initialize output dir just once at start of execution.
    initOutputDirectory("");

#if defined(DBG_BO_TODO) || 1
    // BOBO: xxx this function should be placed here or not?
    dumpConcolicData();
#endif
}

void RuntimeEnv::addPrologRegs(void *env_cpuState, int is_valid)
{
#if defined(CRETE_DEBUG)
	cerr << "[RuntimeEnv] addPrologRegs is invoked. is_valid = " << is_valid << endl;
#endif // defined(CRETE_DEBUG)
	if(!is_valid) {
		m_prolog_regs.push_back(NULL);
		return;
	}

	assert(env_cpuState);

	uint64_t size_regs= CPU_NB_REGS * sizeof(target_ulong);
	uint8_t *new_regs = new uint8_t [size_regs];
	m_prolog_regs.push_back((void *)new_regs);

	CPUState *src_cpuState = (CPUState *) env_cpuState;
	void* src_regs = (void *)src_cpuState->regs;
	void* dst_regs= m_prolog_regs.back();

	memcpy(dst_regs, src_regs, size_regs);
}

// add an empty MemoSyncTable to m_memoSyncTables which will store
// all the load memory operations for an interested TB
void RuntimeEnv::addMemoSyncTable()
{
#if defined(CRETE_DEBUG)
	if(!m_memoSyncTables.empty()){
		cerr << "verifying MemoSyncTable, size = " << dec << m_memoSyncTables.size() << endl;
		verifyMemoSyncTable(m_memoSyncTables.back());
	}
#endif

	memoSyncTable_ty temp_memoSyncTable;
	m_memoSyncTables.push_back(temp_memoSyncTable);

#if defined(CRETE_DEBUG)
	cerr << "[Ld Memo Monitor]addMemoSyncTable is invoked. Number of Memo Sync Table is "
			<< m_memoSyncTables.size() << endl;
#endif // defined(CRETE_DEBUG)
}

// add an memoSyncTableEntry (which is actually a memory operation)
// to the memoSyncTable at the end of m_memoSyncTables
void RuntimeEnv::addMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value)
{
#if defined(CRETE_DEBUG)
	cerr << "[Ld Memo Monitor] addMemoSyncTableEntry is invoked.\n";
#endif // defined(CRETE_DEBUG)

	assert(size <= 8 && "[CRETE ERROR] Data size dumped from qemu is more than 8 bytes!\n");
	memoSyncTable_ty &last_memoSyncTable = m_memoSyncTables.back();

	vector<uint8_t> v_value;
	for(uint32_t i = 0; i < size; ++i) {
		uint8_t temp_value = (value >> i*8) & 0xff;
		v_value.push_back(temp_value);
	}

	addMemoSyncTableEntryInternal(addr, size, v_value, last_memoSyncTable);

#if defined(CRETE_DEBUG)
	cerr << "[Ld Memo Monitor] After insert current MemoSyncTable size is " << last_memoSyncTable.size() << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::addMemoSyncTableEntryInternal(uint64_t addr, uint32_t size, vector<uint8_t> v_value,
		 memoSyncTable_ty& target_memoSyncTable)
{
	assert(v_value.size() == size);

	//Find the existing entries that overlap with the new entry
	vector<uint64_t> overlapped_entries= overlapsMemoSyncEntry(addr, size, target_memoSyncTable);

	// If there is no overlaps, just insert the new entry
	if(overlapped_entries.empty()){
		ConcreteMemoInfo entry_memoSyncTable(addr, size, v_value);
		assert(!m_memoSyncTables.empty());

		target_memoSyncTable.insert(pair<uint64_t, ConcreteMemoInfo>(addr, entry_memoSyncTable));
	} else {
		// If there are overlaps, combine them, including the new given one, to one entry while keeping values
		// of the overlapped bytes from existing entries

		// The address of the new merged entry should be the smallest address
		uint64_t first_overlapped_addr = overlapped_entries.front();
		uint64_t new_merged_addr = (first_overlapped_addr < addr) ?
				first_overlapped_addr : addr;

		uint64_t last_overlapped_addr = overlapped_entries.back();
		uint64_t last_overlapped_size = target_memoSyncTable.find(last_overlapped_addr)->second.m_size;
		uint64_t new_merged_end_addr = ((last_overlapped_addr + last_overlapped_size) > (addr + size) ) ?
				(last_overlapped_addr + last_overlapped_size) : (addr + size);

		uint32_t new_merged_size = new_merged_end_addr - new_merged_addr;
		vector<uint8_t> new_merged_value;

//		cerr << "new_merged_addr = 0x" << hex << new_merged_addr
//				<< ", new_merged_size = 0x" << new_merged_size << endl;

		// Write values for new merged entry
		uint64_t current_writing_byte_addr = new_merged_addr; // The address of the byte that is writing to
		for(vector<uint64_t>::iterator it = overlapped_entries.begin();
				it != overlapped_entries.end(); ++it) {
			memoSyncTable_ty::iterator it_memo_entry = target_memoSyncTable.find(*it);
			assert(it_memo_entry != target_memoSyncTable.end());
			uint64_t exist_entry_addr = it_memo_entry->second.m_addr;
			uint32_t exist_entry_size = it_memo_entry->second.m_size;
			const vector<uint8_t>& exist_entry_value = it_memo_entry->second.m_data;

//			cerr << "1. exist entry: (0x" << hex << exist_entry_addr << ", 0x" << exist_entry_size << ")" << endl;

			// If the current_writing_byte_addr is smaller, that means we need to get the missing bytes from
			// the new given entry
			if (current_writing_byte_addr != exist_entry_addr) {
//				cerr << "2.1 Get value from new entry.\n";
				assert(current_writing_byte_addr < exist_entry_addr);
				while(current_writing_byte_addr < exist_entry_addr){
					uint32_t offset_given_entry = current_writing_byte_addr - addr;
					new_merged_value.push_back(v_value[offset_given_entry]);
					++current_writing_byte_addr;
				}
			}

//			cerr << "2.2 Get value from exist entry.\n";
			assert(current_writing_byte_addr == exist_entry_addr);
			while(current_writing_byte_addr < exist_entry_addr + exist_entry_size){
				uint32_t offset_exist_entry = current_writing_byte_addr - exist_entry_addr;
				assert(offset_exist_entry >= 0 && offset_exist_entry < exist_entry_size);
				new_merged_value.push_back(exist_entry_value[offset_exist_entry]);
				++current_writing_byte_addr;
			}

//			cerr << "3. Erase" << endl;
			// Delete the overlapped entry
			target_memoSyncTable.erase(it_memo_entry);
		}

		if(current_writing_byte_addr < new_merged_end_addr) {
			assert( current_writing_byte_addr < (addr + size) );

			while( current_writing_byte_addr < (addr + size) ){
				uint32_t offset_given_entry = current_writing_byte_addr - addr;
				new_merged_value.push_back(v_value[offset_given_entry]);
				++current_writing_byte_addr;
			}
		}

//		cerr << "4. check for addMemoSyncTableEntryInternal\n";

		assert(new_merged_value.size() == new_merged_size);
		assert(overlapsMemoSyncEntry(addr, size, target_memoSyncTable).empty() &&
				" There should be no overlapped entries anymore after the merge" );
		// Insert the new merged entry to
		ConcreteMemoInfo entry_memoSyncTable(new_merged_addr, new_merged_size, new_merged_value);
		assert(!m_memoSyncTables.empty());

		target_memoSyncTable.insert(pair<uint64_t, ConcreteMemoInfo>(new_merged_addr, entry_memoSyncTable));
	}
}


// Check whether the given entry (addr, size) overlaps with existing entries in target_memoSyncTable
// Return the the address of all the overlapped entries
vector<uint64_t> RuntimeEnv::overlapsMemoSyncEntry(uint64_t addr, uint32_t size,
		memoSyncTable_ty target_memoSyncTable)
{
#if defined(CRETE_DEBUG)
	cerr << "[Ld Memo Monitor] overlapsMemoSyncEntry() is invoked."
			<< "(0x " << hex << addr << ", 0x " << dec << size << ")" << endl;
	cerr << "Exsiting Entries: target_memoSyncTable.size() = " << dec << target_memoSyncTable.size() << endl;
	for(memoSyncTable_ty::iterator temp_it = target_memoSyncTable.begin();
			temp_it != target_memoSyncTable.end(); ++temp_it){
		cerr << hex << "(0x " << temp_it->second.m_addr << ", 0x" << temp_it->second.m_size << "), ";
	}
	cerr << endl;
#endif // defined(CRETE_DEBUG)

	if(target_memoSyncTable.empty())
		return vector<uint64_t>();

	// Find the last entry that goes before given address, "last before" entry, if found
	// If all the entries goes after the given address, it_last_before points to the first entry
	memoSyncTable_ty::iterator it_last_before = target_memoSyncTable.begin();
	for(memoSyncTable_ty::iterator temp_it = target_memoSyncTable.begin();
			temp_it != target_memoSyncTable.end(); ++temp_it) {
		if(temp_it->first < addr) {
			it_last_before = temp_it;
//			cerr << "entry goes before is found: (0x " << it_last_before->second.m_addr
//					<< ", 0x " << it_last_before->second.m_size << ")\n";
		} else {
			break;
		}
	}

#if defined(CRETE_DEBUG)
	cerr << "it_last_before->first: (0x " << hex << it_last_before->first
			<< ", 0x " << it_last_before->second.m_addr
			<< ", 0x " << it_last_before->second.m_size << ")"<< endl;
#endif

	vector<uint64_t> ret_addrs;
	// Check the coming entries one by one to see whether they overlaps with the given entry
	for(memoSyncTable_ty::iterator it_temp = it_last_before;
			it_temp !=  target_memoSyncTable.end(); ++it_temp){
		// if the current one overlaps, return it
		if( (it_temp->second.m_addr + it_temp->second.m_size > addr) &&
				(it_temp->second.m_addr < addr + size) ) {
			ret_addrs.push_back(it_temp->second.m_addr);

//			cerr << "overlaps: addr = 0x" << hex << it_temp->second.m_addr
//					<< ", size = " << dec <<  it_temp->second.m_size << endl;
		}

		if(it_temp->second.m_addr >= (addr + size)){
			break;
		}
	}

#if defined(CRETE_DEBUG)
	cerr << "[Ld Memo Monitor] overlapsMemoSyncEntry() is finished. ret_addrs.size() = "
			<< hex << ret_addrs.size()<< endl;
#endif // defined(CRETE_DEBUG)

	return ret_addrs;
}

void RuntimeEnv::addMemoMergePoint(MemoMergePoint_ty type_MMP)
{
	if(m_memoMergePoints.empty()) {
        assert(flag_interested_tb == 1);
#if defined(CRETE_DEBUG)
		cerr << "[Ld Memo Monitor] First interested TB's mergePoint type is " << type_MMP << endl;
#endif // defined(CRETE_DEBUG)
	}

	if(type_MMP == NormalTb || type_MMP == BackToInterestTb){
		assert(flag_interested_tb == 1);
		m_memoMergePoints.push_back(type_MMP);

#if defined(CRETE_DEBUG)
	cerr << "[addMemoMergePoint] " << m_memoMergePoints.size()
			<< ", merge point type is " << type_MMP << endl;
#endif // defined(CRETE_DEBUG)

		return;
	}

	assert(type_MMP == OutofInterestTb);
	assert(flag_interested_tb == 0);

	// For the situation of OutofInterestTb, we need to update the last element in m_memoMergePoints
	MemoMergePoint_ty &current_tb_MMP = m_memoMergePoints.back();

	if (current_tb_MMP ==  NormalTb)
		current_tb_MMP = OutofInterestTb;
	else if (current_tb_MMP ==  BackToInterestTb)
		current_tb_MMP = OutAndBackTb;
	else
		assert(0 && "[CRETE ERROR] Unexpected type of MemoMergePoint_ty\n");

#if defined(CRETE_DEBUG)
	cerr << "[addMemoMergePoint] size = " << m_memoMergePoints.size()
			<< ", merge point type is " << current_tb_MMP << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::addTBExecSequ(TranslationBlock *tb)
{
    string func_name = tb->llvm_function->getName().str();
    m_tbExecSequ.push_back(func_name);
    m_tbDecl.insert(func_name);

#if defined(HAVLICEK_TB_GRAPH)
    addTBGraphInfo(tb);
#endif // defined(HAVLICEK_TB_GRAPH)

#if defined(CRETE_DEBUG)
    cerr << "[addTBExecSequ] m_tbExecSequ.size() = " << m_tbExecSequ.size()
    		<< ", func_name = " << func_name << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::addMemoStr(string str_memo, DumpMemoType memo_type)
{
#if defined(CRETE_UNUSED_CODE) && 0
	if (memo_type == ConcreteMemo)
		m_concMemos.push_back(str_memo);
	else
		m_symbMemos.push_back(str_memo);
#else
	assert(memo_type == SymbolicMemo);
	m_symbMemos.push_back(str_memo);
#endif
}

void RuntimeEnv::addConcolicData(ConcolicMemoryObject& cmo)
{
    m_makeConcolics.push_back(cmo);

#if defined(CRETE_DEBUG)
    cout << "adding concolic: " << cmo.name_ << endl;
    cout << "makeconcolics size after pushback: " << m_makeConcolics.size() << endl;
#endif // defined(CRETE_DEBUG)
}

static void dump_memory_object(string name,
                               uint64_t name_addr,
                               string value,
                               uint64_t size,
                               target_ulong guest_addr,
                               uint64_t host_addr,
                               RuntimeEnv::DumpMemoType motype,
                               RuntimeEnv& runtime)
{
    stringstream ss(ios_base::app | ios_base::out);
    // Old/concrete format:
    if(motype == RuntimeEnv::ConcreteMemo)
    {
        ss << name
           << " "
           << value
           << " "
           << size
           << ", "
           << guest_addr
           << ':'
           << host_addr
           << '\n';
    }
    else
    {
        // TODO: reduce field number
        // New/symbolic format:
        ss << name
           << ' '
           << name_addr
           << ' '
           << value
           << ' '
           << size
           << ' '
           << guest_addr
           << ' '
           << host_addr
           << '\n';
    }

#if defined(CRETE_DEBUG)
    cout << "mo: " << ss.str();
#endif // defined(CRETE_DEBUG)
    runtime.addMemoStr(ss.str(), motype);
}

#if defined(DBG_BO_TODO) || 1
pair<uint64_t, vector<uint8_t> > guest_read_buf(uint64_t addr, uint64_t size, void* env_cpuState)
{
#if defined(CRETE_DEBUG)
    cout << "guest_read_buf: " << size << " " << hex << addr << dec << endl;
#endif // defined(CRETE_DEBUG)

    pair<uint64_t, vector<uint8_t> > res;
    vector<uint8_t>& v = res.second;

    for(target_ulong i = 0; i < size; ++i)
    {
        uint64_t iaddr = addr + i;
        uint64_t haddr = runtime_env->getHostAddress(env_cpuState, iaddr, 1, 0);
        if(i == 0)
            res.first = haddr;
        uint8_t* ptr = (uint8_t*)haddr;

#if defined(CRETE_DEBUG)
        cout  << "haddr: " << haddr << endl;
        cout  << "*ptr: " << *ptr << endl;
#endif // defined(CRETE_DEBUG)

        v.push_back(*ptr);
    }

    assert(!v.empty());

    return res;
}
#endif //#if defined(DBG_BO_TODO) || 1

void RuntimeEnv::dumpConcolicData()
{
#if defined(CRETE_DEBUG)
    cout << "havlicek concolics size: " << m_makeConcolics.size() << endl;
#endif // defined(CRETE_DEBUG)

    for(vector<ConcolicMemoryObject>::iterator iter = m_makeConcolics.begin();
        iter != m_makeConcolics.end();
        ++iter)
    {
        dump_memory_object(iter->name_,
                           iter->name_addr_,
                           "0",
                           iter->data_size_,
                           iter->data_guest_addr_,
                           iter->data_host_addr_,
                           RuntimeEnv::SymbolicMemo,
                           *this);
    }
}


// Generate output files for runtime environment
void RuntimeEnv::writeRtEnvToFile(const string& outputDirectory)
{
//	initOutputDirectory(outputDirectory);
	writeLlvmMainFunction();
//	writeDebugToFile();
    writeSymbolicMemo();
	writePrologRegs();
	writeMemoSyncTables();
	writeInterruptStates();
	writeTcgLlvmCtx();

#if defined(DBG_TCG_LLVM_OFFLINE)
	tcg_llvm_ctx->writeBitCodeToFile(getOutputFilename("dump_llvm_online.bc"));
#endif

#if defined(HAVLICEK_TB_GRAPH)
    writeTBAddresses();
    writeNewTraceLog();
    writeTBBranchResults();
#endif // defined(HAVLICEK_TB_GRAPH)

#if defined(CRETE_UNUSED_CODE) && 0
	/* BOBO:xxx this should be removed when the corresponding cleanup in dispatch is done*/
	ofstream o_concMos(getOutputFilename("dump_mo_concretes.txt").c_str());
	o_concMos << "Blank: to be remoeved.\n";
	o_concMos.close();

	ofstream o_ebps(getOutputFilename("dump_tbPrologue_ebps.txt").c_str());
	o_ebps<< "Blank: to be remoeved.\n";
	o_ebps.close();

	ofstream o_esps(getOutputFilename("dump_tbPrologue_esps.txt").c_str());
	o_esps<< "Blank: to be remoeved.\n";
	o_esps.close();

	ofstream o_elf(getOutputFilename("dump_elf.bin").c_str());
	o_elf<< "Blank: to be remoeved.\n";
	o_elf.close();

	writeConcreteMemo();
	writeTbPrologue();

	writeTlbTables();
	writePrologTlbTables();
#endif
}

void RuntimeEnv::printInfo()
{
#if defined(CRETE_DEBUG)
    static unsigned current_dump_id = 0;
	cerr << "========================================\n"
			<< "Statistic of run-time environment dump:\n"
            << "Dump ID: " << current_dump_id++ << '\n'
			<< "Number of CPUStates: " << m_cpuStates.size() << '\n'
			<< "Number of m_symbMemos: " << m_symbMemos.size() << '\n'
			<< "Number of m_tbExecSequ: " << m_tbExecSequ.size() << '\n'
			<< "Number of m_tbDecl: " << m_tbDecl.size() << '\n'
			<< "Number of m_interruptStates: " << m_interruptStates.size() << '\n'
			<< "m_outputDirectory: " << m_outputDirectory << '\n'
			<< "========================================\n";
#endif // defined(CRETE_DEBUG)
}

string RuntimeEnv::getOutputFilename(const string &fileName) {
	llvm::sys::Path filePath(m_outputDirectory);
	filePath.appendComponent(fileName);
	return filePath.str();
}

llvm::raw_ostream* RuntimeEnv::openOutputFile(const string &fileName)
{
    string path = getOutputFilename(fileName);
    string error;
    llvm::raw_fd_ostream *f = new llvm::raw_fd_ostream(path.c_str(), error, llvm::raw_fd_ostream::F_Binary);

    if (!f || error.size()>0) {
        llvm::errs() << "Error opening " << path << ": " << error << "\n";
        exit(-1);
    }

    return f;
}


void RuntimeEnv::feed_test_case(const string& file_name)
{
    using namespace std;
    using namespace crete;
    ifstream inputs(file_name.c_str(), ios_base::in | ios_base::binary);

    assert(inputs && "failed to open input argument file!");

#if defined(CRETE_DEBUG)
    cout << "feeding test case" << endl;
#endif // defined(CRETE_DEBUG)

    TestCase tc = read_test_case(inputs);

    size_t found_inputs = 0;
    for(vector<TestCaseElement>::const_iterator tc_iter = tc.get_elements().begin();
        tc_iter !=  tc.get_elements().end();
        ++tc_iter)
    {
        string name(tc_iter->name.begin(), tc_iter->name.end());
        const vector<uint8_t>& value = tc_iter->data;

        for(vector<ConcolicMemoryObject>::const_iterator iter = m_makeConcolics.begin();
            iter != m_makeConcolics.end();
            ++iter)
        {
#if defined(CRETE_DEBUG)
            cout << "checking: " << iter->name_ << endl;
#endif // defined(CRETE_DEBUG)
            if(name == iter->name_)
            {
                assert(value.size() == iter->data_size_);

#if defined(CRETE_DEBUG)
                cout << "replacing current with: " << name << " " << string(value.begin(), value.end()) << endl;
#endif // defined(CRETE_DEBUG)
                memcpy((void*)iter->data_host_addr_, (void*)value.data(), value.size());

                ++found_inputs;
            }
        }
    }
    assert(found_inputs > 0);
    //    assert(found_inputs == tc.get_elements().size()); TODO: unchecked for now because this is called each time a make_concolic is called, so the size will not be same.
}

void RuntimeEnv::dump_initial_input()
{
    crete::TestCase tc;

    for(vector<ConcolicMemoryObject>::iterator iter = m_makeConcolics.begin();
        iter != m_makeConcolics.end();
        ++iter)
    {
        crete::TestCaseElement tce;
        tce.name = vector<uint8_t>(iter->name_.begin(), iter->name_.end());
        tce.name_size = iter->name_.size();
        tce.data = vector<uint8_t>(iter->data_size_, 0);
        tce.data_size = iter->data_size_;

        tc.add_element(tce);
    }

    // Must write this for Klee to pick up because the initial test case is missing.
    ofstream ofs("hostfile/input_arguments.bin", ios_base::out | ios_base::binary);
    assert(ofs);
    tc.write(ofs);

#if defined(CRETE_DEBUG)
    cout << "dumping initial input to file" << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::initOutputDirectory(const string& outputDirectory)
{
	if (outputDirectory.empty()) {
		llvm::sys::Path cwd = llvm::sys::Path::GetCurrentDirectory();

		for (int i = 0; ; i++) {
            ostringstream dirName;
            dirName << "runtime-dump-" << i;

            llvm::sys::Path dirPath(cwd);
            dirPath.appendComponent("trace");
            dirPath.appendComponent(dirName.str());

            bool exists = false;
            llvm::sys::fs::exists(dirPath.str(), exists);

            if(!exists) {
                m_outputDirectory = dirPath.str();
                break;
            }
        }

    } else {
        m_outputDirectory = outputDirectory;
    }

#if defined(CRETE_DEBUG)
	cerr << "Output directory = \" " << m_outputDirectory << "\"\n";
	printf("printf: Output directory = \" %s\".\n", m_outputDirectory.c_str());
#endif // defined(CRETE_DEBUG)

    llvm::sys::Path outDir(m_outputDirectory);
    string mkdirError;

    if (outDir.createDirectoryOnDisk(true, &mkdirError)) {
#if defined(CRETE_DEBUG)
        cerr << "Could not create output directory " << outDir.str() <<
                " error: " << mkdirError << '\n';
#endif // defined(CRETE_DEBUG)
        exit(-1);
    }

    llvm::sys::Path dumpLast("trace");
    dumpLast.appendComponent("runtime-dump-last");

    if ((unlink(dumpLast.c_str()) < 0) && (errno != ENOENT)) {
        perror("ERROR: Cannot unlink runtime-dump-last");
        exit(1);
    }

    if (symlink(m_outputDirectory.c_str(), dumpLast.c_str()) < 0) {
        perror("ERROR: Cannot make symlink runtime-dump-last");
        exit(1);
    }

}

void RuntimeEnv::writeDebugToFile()
{
	llvm::raw_ostream* f_debugRaw = openOutputFile("Debug.txt");

	*f_debugRaw << "Execution sequence in TB\n";
    for(vector<string>::iterator it = m_tbExecSequ.begin();
    		it != m_tbExecSequ.end(); ++it) {
    	*f_debugRaw << *it << "\n";
    }

	*f_debugRaw << "LLVM function declarations\n";
    for(set<string>::iterator it = m_tbDecl.begin();
    		it != m_tbDecl.end(); ++it) {
    	*f_debugRaw << *it << "\n";
    }

    delete f_debugRaw;
}

void RuntimeEnv::writeSymbolicMemo()
{
	if(!m_symbMemos.empty()) {
		ofstream o_symbMos(getOutputFilename("dump_mo_symbolics.txt").c_str());

#if defined(CRETE_DEBUG)
		cerr << "writeSymbolicMemo():\n";
#endif // defined(CRETE_DEBUG)
		for(vector<string>::iterator it = m_symbMemos.begin();
				it != m_symbMemos.end(); ++it) {
			o_symbMos << *it;
//			cerr << *it;
		}

		o_symbMos.close();
	}

}

/* The format is:
 * offset in CPUState // 8bytes
 * sizeof(regs_entry) // 8 bytes
 * amount of TBs (amt_tbs)// 8 bytes
 * flag to indicate whether a TB needs to update regs // amt_tb bytes
 * data of dumped regs // (amount of TB needs to update regs) * sizeof(regs) bytes
 * */
void RuntimeEnv::writePrologRegs()
{
	assert(m_prolog_regs.size() == rt_dump_tb_count);

	uint64_t offset_regs = CPU_OFFSET(regs);
	uint64_t size_regs_entry = CPU_NB_REGS * sizeof(target_ulong);
	uint64_t amt_regs = m_prolog_regs.size();

	vector<uint8_t> is_valid;
	is_valid.reserve(amt_regs);

	for(vector<void *>::iterator it = m_prolog_regs.begin();
			it != m_prolog_regs.end(); ++it) {
		if( (*it) != NULL ){
			is_valid.push_back(1);
		} else {
			is_valid.push_back(0);
		}
	}

    ofstream o_regs(getOutputFilename("dump_tbPrologue_regs.bin").c_str(),
    		ios_base::binary);

    o_regs.write((const char*)&offset_regs, sizeof(offset_regs));
    o_regs.write((const char*)&size_regs_entry, sizeof(size_regs_entry));
    o_regs.write((const char*)&amt_regs, sizeof(amt_regs));
    o_regs.write((const char*)is_valid.data(), is_valid.size()*sizeof(uint8_t));

	for(vector<void *>::iterator it = m_prolog_regs.begin();
			it != m_prolog_regs.end(); ++it) {
		if( (*it) != NULL ){
			o_regs.write((const char*)(*it), size_regs_entry);
		}
	}

#if defined(CRETE_DEBUG)
    cerr << "[writePrologRegs] amt_regs (amt_tbs) = " << amt_regs
    		<< ", size_regs_entry = " << size_regs_entry
    		<< "is_valid.size() = " << is_valid.size()
    		<< endl;
#endif // defined(CRETE_DEBUG)

    o_regs.close();
}

#if defined(DBG_BO_TODO) || 1
bool RuntimeEnv::overlaps_with_existing_mo(uint64_t addr, size_t size)
{
    for(vector<ConcolicMemoryObject>::iterator iter = m_makeConcolics.begin();
        iter != m_makeConcolics.end();
        ++iter)
    {
        if((iter->data_guest_addr_ < addr &&
           iter->data_guest_addr_ + iter->data_size_ > addr)
           ||
           (addr < iter->data_guest_addr_ &&
           addr + size > iter->data_guest_addr_))
        {
            return true;
        }
    }

    return false;
}
#endif

// Write main function for off-line replay in the format of llvm
// BOBO: xxx a better way to do it is constructing a llvm::function and then write it to file
void RuntimeEnv::writeLlvmMainFunction()
{
	if(m_cpuStates.empty()) {
#if defined(CRETE_DEBUG)
		cerr << "Error: No CPUStates have been dumped.\n";
#endif // defined(CRETE_DEBUG)
        return;//exit(-1);
	}

	assert(m_tbExecSequ.size() == rt_dump_tb_count);

    ofstream o_mainFunc(getOutputFilename("main_function.ll").c_str());
#if defined(TARGET_X86_64)
    string file_name = crete_find_file(CRETE_FILE_TYPE_LLVM_TEMPLATE, "crete-qemu-1.0-template-main-i386.ll");
#else
    string file_name = crete_find_file(CRETE_FILE_TYPE_LLVM_TEMPLATE, "crete-qemu-1.0-template-main-i386.ll");
#endif // defined(TARGET_X86_64)
    ifstream i_template(file_name.c_str());

	assert(o_mainFunc);
	assert(i_template && "template_main.ll is missing in the current folder.\n");

    string content_line;

	// The first line of template file, which is a comments in llvm-bitcode,
	// should be putted in the harness file and should not be reoved
	getline(i_template, content_line, '\n');
	o_mainFunc << content_line <<'\n';

	// The delimiter used here is ";" which is the char used in llvm-bitcode for comments
	while(getline(i_template, content_line, ';')) {
		o_mainFunc << content_line;

		// Get the contennts of comments
		getline(i_template, content_line, '\n');
		// when the comment is: @tcg_env:
		//  @tcg_env should be dumped and injected into the harness file
		if( content_line.compare(0, 10, " @tcg_env:") == 0 ) {
			uint64_t size_env = sizeof(CPUState);
			assert((size_env % sizeof(uint64_t)) == 0 );
            uint64_t size_temp = size_env / sizeof(uint64_t);
            uint64_t* temp_dump = (uint64_t *)m_cpuStates[0];
            uint64_t i;

            o_mainFunc << "@tcg_env = global [" << size_temp <<" x i64] [";

            for(i = 0; i < size_temp - 1; ++i)
            	o_mainFunc << "i64 " << temp_dump[i] << ", ";

            o_mainFunc << "i64 " << temp_dump[i] << "], align 16\n";
		}
		else if( content_line.compare(0, 10, " call_tcg:") == 0 ) {
			uint64_t call_index = 2;
			uint64_t tb_prelogue_index = 0;

		    for(vector<string>::iterator it = m_tbExecSequ.begin();
		    		it != m_tbExecSequ.end(); ++it) {
		    	o_mainFunc << "call void @qemu_tb_prelogue(i64 " << tb_prelogue_index <<")\n";
		    	o_mainFunc << "%" << call_index << " = call i64 @" << *it <<"(i64* %1)\n";

		    	++call_index;
		    	++tb_prelogue_index;
		    }

		}
		else if( content_line.compare(0, 12, " declare_tcg") == 0 ) {
			o_mainFunc << "declare void @qemu_tb_prelogue(i64)\n";

		    for(set<string>::iterator it = m_tbDecl.begin();
		    		it != m_tbDecl.end(); ++it) {
				o_mainFunc << "declare i64 @" << *it <<"(i64*)\n";
		    }

		}
	}

	o_mainFunc.close();
	i_template.close();
}

/*
 * Read a register value from the given CPUState
 * env_cpuState: given CPUState
 * buf: store the result
 * offset: the offset from the beginning of CPUState
 * size: in terms of Bytes
 */
void RuntimeEnv::readCpuRegister(void *env_cpuState, void* buf,
		uint32_t offset, uint32_t size)
{
	assert(size <= 8 || offset > 0);
	assert( offset + size <= sizeof(CPUState));

	uint8_t *source_ptr = (uint8_t *) env_cpuState + offset;
	memcpy(buf, source_ptr, size);
}

/*
 * Get the corresponding host (virtual) address for a given guest (virtual) address
 * This function is written based on "__ldl_mmu" in file "softmmu_template.h"
 * BOBO: Call tlb_fill if the given guest address is not found in the TLB table
 *           in the current CPUState.
 */
uint64_t RuntimeEnv::getHostAddress(void *env_cpuState, uint64_t addr, int mmu_idx, int is_write)
{
	flag_getHostAddress = 1;

	bool use_new_cpuState = false;

	target_ulong tlb_addr;
    int index;
    unsigned long addend;
    uint64_t ret;

    CPUState *env_ptr = (CPUState *) env_cpuState;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
 redo:
    if(is_write)
    	tlb_addr = env_ptr->tlb_table[mmu_idx][index].addr_write;
    else
    	tlb_addr = env_ptr->tlb_table[mmu_idx][index].addr_read;

#if defined(CRETE_DEBUG) && 0
    uint64_t tlb_cond_1 = (addr & TARGET_PAGE_MASK);
    uint64_t tlb_cond_2 = (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK));

    bool tlb_condition = (tlb_cond_1 == tlb_cond_2);

    cout << "tlb_cond_1 = " << tlb_cond_1 << ", tlb_cond_2 = " << tlb_cond_2
    		<<", tlb_condition = " << tlb_condition << endl
    		<< "index = " << index << ", tlb_addr = " << tlb_addr << endl;
#endif // defined(CRETE_DEBUG)

    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
        	assert(0 && "IO access in getHostAddress.\n");
        }
        else {
        	/* aligned/unaligned access in the same page */
        	addend = env_ptr->tlb_table[mmu_idx][index].addend;
        	ret = addr + addend;
        }
    } else {
        /* the page is not in the TLB : Call tlb_fill to refill it
         * BOBO: In order to avoid changing the CPUState used by QEMU, a new CPUState is created
         *       by copying the current CPUState
    	 */
    	assert(!use_new_cpuState && "tlb_fill in getHostAddress fails!\n");

    	if(!use_new_cpuState) {
    		CPUState *src_cpuState = (CPUState *) env_cpuState;
    		assert(src_cpuState);
    		CPUState *dst_cpuState = (CPUState *) new uint8_t [sizeof(CPUState)];

#if defined(CRETE_DEBUG)
    		cerr << "new CPUState is allocated in getHostAddress, and its addr is "
    				<< hex << (uint64_t)dst_cpuState<< "\n" << dec;
#endif // defined(CRETE_DEBUG)

    		memcpy(dst_cpuState, src_cpuState, sizeof(CPUState));

        	env_ptr = dst_cpuState;
        	use_new_cpuState = true;
    	}

    	// is_write = 0, retaddr = NULL
        tlb_fill(env_ptr, addr, is_write, mmu_idx, 0);

        goto redo;
    }

    if(use_new_cpuState) {
#if defined(CRETE_DEBUG) && 0
    	cerr << "[Fixed]: tlb_fill in getHostAddress works!\n"
    			<< hex << addr << ":" << ret << dec << endl;
#endif // defined(CRETE_DEBUG)

    	delete (uint8_t *)env_ptr;
    }

    flag_getHostAddress = 0;

    return ret;
}

// merge a sequence of consecutive non-BackToInterestTb TB's memoSyncTables
//  to the nearest BackToInterestTb TB's memoSyncTable and make them empty
void RuntimeEnv::mergeMemoSyncTables()
{
	debugMergeMemoSync();

	assert(m_memoSyncTables.size() == rt_dump_tb_count);
	assert(m_memoMergePoints.size() == rt_dump_tb_count);

	assert(m_memoMergePoints.front() == BackToInterestTb ||
			m_memoMergePoints.front() == OutAndBackTb);
	assert(m_memoMergePoints.back() == OutofInterestTb ||
			m_memoMergePoints.back() == OutAndBackTb);

	memoSyncTable_ty *dst_mst = 0;
	memoSyncTable_ty *src_mst = 0;

	for(uint64_t i = 0; i < m_memoSyncTables.size(); ++i) {
		if(m_memoMergePoints[i] == BackToInterestTb) {
			// the BackToInterestTb TB's memoSyncTable will be used as destination for the merging
			dst_mst = &m_memoSyncTables[i];
			continue;
		} else if ( m_memoMergePoints[i] == OutAndBackTb) {
			dst_mst = 0;
			continue;
		}

		// the NormalTb and OutofInterestTb TB's memoSyncTable will be used as source and will be cleared
		assert(m_memoMergePoints[i] == NormalTb ||
				m_memoMergePoints[i] == OutofInterestTb);

		src_mst = &m_memoSyncTables[i];
		assert(dst_mst && src_mst);

		//insert source memorySyncTable to destination memorySyncTable
		for(memoSyncTable_ty::iterator it = src_mst->begin();
						it != src_mst->end(); ++it){
			uint64_t src_entry_addr = it->second.m_addr;
			uint64_t src_entry_size = it->second.m_size;
			vector<uint8_t> src_entry_value = it->second.m_data;
			addMemoSyncTableEntryInternal(src_entry_addr, src_entry_size, src_entry_value, *dst_mst);
		}

		src_mst->clear();
		assert(m_memoSyncTables[i].empty());
	}
}

/*
 * Format of file "dump_sync_memos.bin"
 * - amount of dumped TBs (amt_dumped_tbs)// 8 bytes
 * - flag to indicate whether a TB needs to do MemoSync (flags_need_memo_sync)// amt_dumped_tbs bytes
 * - data of m_memoSyncTables
 * format of m_memoSyncTables
 * - amount of non-empty memoSyncTable (amt_memoSyncTables)// 8 bytes
 * - data of each non-empty memoSyncTable
 * format of each non-empty memoSyncTable
 * - amount of ConcreteMemoInfo entries (amt_memo_entries) // 8 bytes
 * - data of each ConcreteMemoInfo entry
 * format of each ConcreteMemoInfo entry
 * - address (addr_memo_sync)// 8bytes
 * - data_size (size_memo_sync)// 4 bytes
 * - data (data_memo_sync)// data_size bytes
 * */
void RuntimeEnv::writeMemoSyncTables()
{
#if defined(CRETE_DEBUG)
	cerr << "Verifying all m_memoSyncTables before merging memoSyncTables: \n";
	for(memoSyncTables_ty::const_iterator const_it = m_memoSyncTables.begin();
			const_it != m_memoSyncTables.begin(); ++const_it) {
		verifyMemoSyncTable(*const_it);
	}
#endif

	mergeMemoSyncTables();

#if defined(CRETE_DEBUG)
	cerr << "[m_memoMergePoints] " << endl;
	for(uint64_t i = 0; i < m_memoMergePoints.size(); ++i) {
		cerr << "TB_" << i <<": Merge Type = " << m_memoMergePoints[i]
             << ", memoSyncTable size = " << m_memoSyncTables[i].size() << endl;
	}
#endif // defined(CRETE_DEBUG)

	// double check the merge is correct
	for(uint64_t i = 0; i < m_memoMergePoints.size(); ++i) {
		if(m_memoMergePoints[i] == OutofInterestTb ||
				m_memoMergePoints[i] == NormalTb)
			assert(m_memoSyncTables[i].empty() && "Something is wrong in mergeMemoSyncTables().\n");
	}

	ofstream o_sm(getOutputFilename("dump_sync_memos.bin").c_str(),
    		ios_base::binary);
	assert(o_sm && "Create file failed: dump_sync_memos.bin\n");

	// - amount of dumped TBs (amt_dumped_tbs)// 8 bytes
	uint64_t amt_dumped_tbs = m_memoMergePoints.size();
	o_sm.write((const char*)&amt_dumped_tbs, sizeof(amt_dumped_tbs));

    vector<uint8_t> flags_need_memo_sync;
    uint64_t amt_memoSyncTables = 0;
	for(uint64_t i = 0; i < amt_dumped_tbs; ++i) {
		if(m_memoSyncTables[i].empty()) {
			flags_need_memo_sync.push_back(0);
		} else {
			flags_need_memo_sync.push_back(1);
			++amt_memoSyncTables;
		}
	}
	assert(flags_need_memo_sync.size() == amt_dumped_tbs);
	// - flag to indicate whether a TB needs to do MemoSync (flags_need_memo_sync)// amt_dumped_tbs bytes
	o_sm.write((const char*)flags_need_memo_sync.data(), flags_need_memo_sync.size()*sizeof(uint8_t));
	// - amount of non-empty memoSyncTable (amt_memoSyncTables)// 8 bytes
	o_sm.write((const char*)&amt_memoSyncTables, sizeof(amt_memoSyncTables));

	// write data of all non-empty memoSyncTables to file
	for(uint64_t i = 0; i < amt_dumped_tbs; ++i) {
		if(!m_memoSyncTables[i].empty()) {
			uint64_t amt_memo_entries = m_memoSyncTables[i].size();
			// - amount of ConcreteMemoInfo entries (amt_memo_entries) // 8 bytes
			o_sm.write((const char*)&amt_memo_entries, sizeof(amt_memo_entries));

			// write data of one memoSyncTable to file
			for(memoSyncTable_ty::iterator it = m_memoSyncTables[i].begin();
					it != m_memoSyncTables[i].end(); ++it) {
				ConcreteMemoInfo v_concMemo_info = it->second;

				uint64_t addr_memo_sync = v_concMemo_info.m_addr;
				uint32_t size_memo_sync = v_concMemo_info.m_size;
				vector<uint8_t> data_memo_sync = v_concMemo_info.m_data;

				// - address (addr_memo_sync)// 8bytes
				o_sm.write((const char*)&addr_memo_sync, sizeof(addr_memo_sync));
				// - data_size (size_memo_sync)// 4 bytes
				o_sm.write((const char*)&size_memo_sync, sizeof(size_memo_sync));
				// - data (data_memo_sync)// data_size bytes
				o_sm.write((const char*)data_memo_sync.data(), data_memo_sync.size()*sizeof(uint8_t));
			}
		}
	}

	o_sm.clear();

#if defined(CRETE_DEBUG)
	print_memoSyncTables();
#endif
}

void RuntimeEnv::debugMergeMemoSync()
{
#if defined(CRETE_DEBUG)
	cerr << "[debugMergeMemoSync]" << endl
			<< "m_tbExecSequ.size() = " << m_tbExecSequ.size() << ", "
			<< "m_memoMergePoints.size() = " << m_memoMergePoints.size() << ", "
			<< "m_memoSyncTables.size() = " << m_memoSyncTables.size() << endl;

	cerr << "\t m_memoMergePoints type: ";

	for(uint64_t i = 0; i < m_memoMergePoints.size(); ++i) {
		cerr << m_memoMergePoints[i] << " ";
	}
	cerr << "\n\t m_memoSyncTables[i].size(): ";
	for(uint64_t i = 0; i < m_memoSyncTables.size(); ++i) {
		cerr << m_memoSyncTables[i].size() << " ";
	}
	cerr << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::verifyMemoSyncTable(const memoSyncTable_ty& target_memoSyncTable)
{
	for(memoSyncTable_ty::const_iterator it = target_memoSyncTable.begin();
			it != target_memoSyncTable.end(); ++it) {
		if(it->first != it->second.m_addr) {
			cerr << hex << "it->first = 0x" << it->first << "it->second.m_addr = 0x" << it->second.m_addr << endl;
			assert(0 && "verifyMemoSyncTable() failed.\n");
		}
	}
}

void RuntimeEnv::print_memoSyncTables()
{
#if defined(CRETE_DEBUG)
	cerr << "[Memo Sync Table]\n";

	for(memoSyncTables_ty::iterator it = m_memoSyncTables.begin();
			it != m_memoSyncTables.end(); ++it){
		if(it->empty()){
			cerr << "NULL\n";
		} else {
			cerr << it->size() << ": " << hex;
			for(memoSyncTable_ty::iterator m_it = it->begin();
					m_it != it->end(); ++m_it){
				cerr << m_it->first << ", ";
			}
			cerr << dec << endl;
		}
	}
#endif // defined(CRETE_DEBUG)
}

#if defined(DBG_BO_CALL_STACK)
/*  A: When flag_holdon_monitor_call_stack = 1:
 *    Check whether the execution of program returns to the function from which the holdon is enabled.
 *  B: When flag_holdon_monitor_call_stack = 0:
 *    Update the call stack based on the input tb_pc:
 *    1. if tb_pc is from the same function as the top entry on the call stack,
 *    the stack needs not to be updated.
 *    2. if tb_pc is not from the same function and equals to the first instruction of
 *    another function in symTab, push a new entry in call stack, because this is a function call.
 *    3. if tb_pc is neither in situation 1 and 2, and also if tb_pc is from the same scope of a
 *    function that is from the current call stack, this is a function return. All the entries above
 *    the entry that is from the function as the current tb-pc will be popped out. The cases that
 *    multiple entries will be popped out is caused by direct jump, like: jmp *eax.
 *    BOBO: xxx the assumption here to work properly for situation 3 is directly jump will not go into
 *              the middle of a function, which means it has to go to the beginning of a function
 *    4. if tb_pc is neither in situation 1/2/3, this means the call stack lost here, could be a function
 *    call that we do not have a corresponding entry in symbol table. In this situation, we will hold
 *    on update on call stack until the execution of program returns to the current function.
 * */
void RuntimeEnv::updateCallStack(uint64_t tb_pc)
{
#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
	cerr << "[call stack] updateCallStack is invoked! tb_pc = " << hex << tb_pc << dec
			<< ", call_stack_size = " << m_callStack.size() << endl;
#endif


	// Waiting for main function to be the base function of call stack
	if(m_callStack.empty()){
		if(tb_pc == addr_main_function) {
			map<uint64_t, CallStackEntry>::iterator it = elf_symtab_functions.find(tb_pc);
			assert((it != elf_symtab_functions.end()) && "main function is not found in the symbolic table\n");
    		m_callStack.push(it->second);

    		call_stack_started = true;
#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
    		cerr<< "\t initial/push stack: " << it->second.m_func_name << endl;
    		cerr << "[call stack] updateCallStack is finished!\n";
#endif
		}
#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
		else {
			cerr << " [call stack] waiting for main: tb_pc = 0x" << hex << tb_pc
					<<", addr_main_function = 0x" << addr_main_function << dec << endl;
		}
#endif

		return;
	}

	CallStackEntry top_callStack = m_callStack.top();
	bool within_same_function = (tb_pc >= top_callStack.m_start_addr)
			&& (tb_pc < (top_callStack.m_start_addr + top_callStack.m_size) );

#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
	cerr << "\t beofre update:\n\t top_callStack: function_name = " << top_callStack.m_func_name
//			<< ", start_addr = 0x" << hex <<  top_callStack.m_start_addr
//			<< ", end_addr = 0x" << top_callStack.m_start_addr + top_callStack.m_size
//			<< ", size = 0x" << top_callStack.m_size
			<< dec << endl;
	cerr << "\t within_same_function = " << within_same_function << endl;
#endif

	// Only check but not do the update when flag_holdon_monitor_call_stack is enabled
	if (flag_holdon_monitor_call_stack == 1) {
#if 1
		// If the function that is the same as waiting function is invoked, we pop the pop entry
		// on call stack to wait the execution go back to the caller of the original waiting function
		if(tb_pc == top_callStack.m_start_addr) {
#if defined(DBG_BO_CURRENT)  && defined(CRETE_DBG_CALL_STACK)
			cerr << "\t [holdon] The same function as waiting funcion is invoked at tb->pc = "
					<< hex << tb_pc << dec << endl;
#endif
			assert(m_callStack.size() >= 2 &&
					"The base function in call stack should never be called when call stack is holdon\n");
			m_callStack.pop();
			return;
		}
#else
		assert(tb_pc != top_callStack.m_start_addr
				&& "A same function is invoked when monitoring call stack is holdon");
#endif


		// When execution of the program return to the function from which the pause on monitoring call
		// stack starts, resume the monitor of call stack.
		if(within_same_function) {
			flag_holdon_monitor_call_stack = 0;

#if defined(DBG_BO_CURRENT)  && defined(CRETE_DBG_CALL_STACK)
			cerr << "======= [holdon] Resume at tb->pc = "
					<< hex << tb_pc << dec << endl;
#endif
		} else if (m_callStack.size() >= 2) {
			//To handle Special case, that is interruption happened at the same time of original function return
			// So that, we check whether the tb_pc is from the second top entry of the call stack
			CallStackEntry temp_entry = m_callStack.top();
			m_callStack.pop();

			CallStackEntry second_top_callStack = m_callStack.top();
			within_same_function = (tb_pc >= second_top_callStack.m_start_addr)
					&& (tb_pc < (second_top_callStack.m_start_addr + second_top_callStack.m_size) );


			if (within_same_function) {
				//If tb_pc is from the second top call stack either, resume the call stack monitoring
				flag_holdon_monitor_call_stack = 0;

#if defined(DBG_BO_CURRENT)  && defined(CRETE_DBG_CALL_STACK)
				cerr << "======= [holdon] Resume at tb->pc = " << hex << tb_pc << dec <<
						", special case for interruption return." << endl;
#endif
			} else {
				// Otherwise, restore the popped entry and continue the hold on call stack monitoring
				m_callStack.push(temp_entry);

#if defined(DBG_BO_CURRENT) && 0
			cerr << "\t [holdon] Continue at tb->pc = "
					<< hex << tb_pc << dec << endl;
#endif
			}
		}
#if defined(DBG_BO_CURRENT) && 0
		else {
			cerr << "\t [holdon] Continue at tb->pc = "
					<< hex << tb_pc << dec << endl;
		}
#endif
		return;
	}

	assert(flag_holdon_monitor_call_stack == 0);
	if(!within_same_function){
    	map<uint64_t, CallStackEntry>::iterator it = elf_symtab_functions.find(tb_pc);
    	if(it != elf_symtab_functions.end()){
    		// Situation 2
    		m_callStack.push(it->second);

#if defined(DBG_BO_CURRENT)  && defined(CRETE_DBG_CALL_STACK)
    		cerr << "\t call/push stack: " << it->second.m_func_name << endl;
#endif
    	} else if ( m_callStack.size() == 1){
#if defined(DBG_BO_DYN_CALL_STACK) || 1
			// Situation 4: special case for base function
			flag_holdon_monitor_call_stack = 1;

#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
			cerr << "======= [holdon] special case for bas: starts at tb->pc = "
					<< hex << tb_pc << dec << endl;
#endif
#else //#if defined(DBG_BO_DYN_CALL_STACK) || 1
    		// special case for base function
    		/* BOBO: xxx assume the base function will only call the function that could be found
    		 *       in symbol table. As a result:
    		 *       1. for base function that is not in situation 1/2 and  current tb_pc is from
    		 *       user level, it has to be situation 3 which is a return. And when base function
    		 *       returns, monitoring on call stack will be stopped for this iteration.
    		 *       2. if the tb_pc is not from user level code, we assume this is an interruption,
    		 *       and hence will pause call stack monitor and waiting for wake up later.
    		 * */
    		if(tb_pc < USER_CODE_RANGE) {
    			// Situation 3: special case for base function
    			m_callStack.pop();
    			assert(m_callStack.size() == 0 && " Call Stack should be empty now.\n");
    			//Disable monitor call stack when the base function returns
    			flag_enable_monitor_call_stack = 0;
    			flag_holdon_monitor_call_stack = 1;

#if defined(DBG_BO_CURRENT) && 0
    		cerr << "++++[call Stack] Monitor Call Stack Finished.\n";
#endif
    		} else {
    			// Situation 4: special case for base function
    			flag_holdon_monitor_call_stack = 1;

#if defined(DBG_BO_CURRENT) && 0
    			cerr << "======= [holdon] special case for bas: starts at tb->pc = "
    					<< hex << tb_pc << dec << endl;
#endif
    		}
#endif //#if defined(DBG_BO_DYN_CALL_STACK)
    	} else {
    		assert(m_callStack.size() >= 2);

    		// Compare the current tb->pc with every entries in the call stack.
    		// if it does not match anyone of them, pause call stack monitor (situation 4)

    		vector<CallStackEntry> temp_entries;

    		bool is_return;

    		while( !m_callStack.empty() ){
        		CallStackEntry current_func = m_callStack.top();
        		is_return = (tb_pc >= current_func.m_start_addr) &&
        				(tb_pc < (current_func.m_start_addr + current_func.m_size));

        		if(is_return) {
        			// Situation 3: normal case of return
#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
        			cerr << "\t return/pop stack:"
        					<< "\' to function \'" << current_func.m_func_name << "\'"<< endl;
        			if(temp_entries.size() > 1) {
        				cerr << "\t direct jump happened and is handled.\n" << endl;
        			}
#endif
        			break;
        		} else {
        			temp_entries.push_back(current_func);
	        		m_callStack.pop();
        		}
    		}

    		if(m_callStack.empty()) {
    			// Situation 4
    			flag_holdon_monitor_call_stack = 1;
    			// Restore the popped entries on call stack
    			for(vector<CallStackEntry>::reverse_iterator rit =  temp_entries.rbegin();
    					rit != temp_entries.rend(); ++rit) {
    				m_callStack.push(*rit);
    			}

#if defined(DBG_BO_CURRENT) && defined(CRETE_DBG_CALL_STACK)
    			cerr << "======= [holdon] starts at tb->pc = "
    					<< hex << tb_pc << dec << endl;
#endif
    		}

    	}
	}
#if defined(DBG_BO_CURRENT)  && defined(CRETE_DBG_CALL_STACK)
	else {
		// situation 1
		cerr << "\t continue the execution of function: " << top_callStack.m_func_name << endl;
	}

	cerr << "[call stack] updateCallStack is finished! call_stack_size = " << m_callStack.size() << endl;
#endif
}

int RuntimeEnv::getCallStackSize() const
{
	return m_callStack.size();
}

#if defined(DBG_BO_DYN_CALL_STACK) || 1
void RuntimeEnv::finishCallStack() {
	if (call_stack_started){
		assert(m_callStack.size() >= 1 && " Call Stack should be empty now.\n");

		m_callStack.pop();
	}

	//Disable monitor call stack when the base function returns
	flag_enable_monitor_call_stack = 0;
	flag_holdon_monitor_call_stack = 1;

#if defined(DBG_BO_CURRENT) && 0
	cerr << "++++[call Stack] Monitor Call Stack Finished after base function returned.\n";
#endif
}
#endif //#if defined(DBG_BO_DYN_CALL_STACK)

void RuntimeEnv::print_callStack()
{

}

void RuntimeEnv::pushInterruptStack(QemuInterruptInfo interrup_info, uint64_t interrupted_pc)
{
	m_interruptStack.push(make_pair(interrup_info, interrupted_pc));

#if defined(CRETE_DEBUG)
	cout << "[interruptStack] pushInterruptStack() is done: interrupted_pc = 0x" << hex << interrupted_pc
			<< ", m_interruptStack.size() = " << dec << m_interruptStack.size() << endl;
#endif
}

void RuntimeEnv::popInterruptStack(uint64_t resumed_pc)
{
//	assert(!m_interruptStack.empty());

	if(m_interruptStack.empty()){
#if defined(CRETE_DEBUG)
		cerr << "[CRETE TODO] popInterruptStack() is invoked when interruptStack is empty, "
				<< "resumed_pc = 0x" << hex << resumed_pc << dec << endl;
#endif // defined(CRETE_DEBUG)

		return;
	}

	assert(resumed_pc == m_interruptStack.top().second &&
			"[interruptStack] mis-match found.\n");

	m_interruptStack.pop();

#if defined(CRETE_DEBUG)
	cout << "[interruptStack] popInterruptStack() is done: resumed_pc = 0x" << hex << resumed_pc
			<< ", m_interruptStack.size() = " << dec << m_interruptStack.size() << endl;
#endif
}

// Return true is m_interruptStack is not empty, which indicates the program is processing interrupt
// rather than executing normal code
bool RuntimeEnv::isProcessingInterrupt()
{
	if(!m_interruptStack.empty()) {
		return true;
	} else {
		return false;
	}
}

#endif //#if defined(DBG_BO_CALL_STACK)

#if defined(DBG_BO_REPLAY_INTERRUPT)
void RuntimeEnv::addQemuInterruptState(QemuInterruptInfo interrup_info)
{
	CPUState *blank_cpuState = (CPUState *) new uint8_t [sizeof(CPUState)];

	m_interruptStates.push_back(make_pair(interrup_info, (void *)blank_cpuState));

#if defined(CRETE_DEBUG)
	cout << "[INT RPL] addQemuInterruptState is invoked." << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::addEmptyQemuInterruptState()
{
	QemuInterruptInfo empty_intterrup_info(0,0,0,0);
	m_interruptStates.push_back(make_pair(empty_intterrup_info, (void *)NULL));

#if defined(CRETE_DEBUG)
    cout << "[INT RPL] addEmptyQemuInterruptState is invoked." << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::dumpCpuStateForInterrupt(void *dumpCpuState)
{
	assert(flag_dump_interrupt_CPUState == 1 &&
			"flag_interrupt_occured should be enabled which indicates a interrupt is just finished.\n");

	assert(!m_interruptStates.empty());

	CPUState *src_cpuState = (CPUState *) dumpCpuState;
	CPUState *dst_cpuState = (CPUState *)m_interruptStates.back().second;
	assert(src_cpuState && dst_cpuState);

	memcpy(dst_cpuState, src_cpuState, sizeof(CPUState));
}

/* The format of "dump_qemu_interrupt_info.bin" is:
 * sizeof(QemuInterruptInfo) (size_QemuInterruptInfo) // 8 bytes
 * sizeof(CPUState) (size_CPUSate) // 8 bytes
 * amount of TBs (amt_tbs)// 8 bytes
 * amount of non-empty interruptStates (amt_interruptStates)// 8 bytes
 * flag to indicate whether a TB had an interrupt state info (is_valid)// amt_tbs bytes
 * data of dumped nonempty interruptState_ty
 * - format of each nonempty entry of interruptState_ty:
 *   data of size_QemuInterruptInfo // size_QemuInterruptInfo bytes
 *   data of CPUState // size_CPUSate bytes
 * */
void RuntimeEnv::writeInterruptStates()
{
	assert(m_interruptStates.size() == rt_dump_tb_count);

	ofstream o_sm(getOutputFilename("dump_qemu_interrupt_info.bin").c_str(),
    		ios_base::binary);
	assert(o_sm && "Create file failed: dump_qemu_interrupt_info.bin\n");

	// sizeof(QemuInterruptInfo) (size_QemuInterruptInfo) // 8 bytes
	uint64_t size_QemuInterruptInfo= sizeof(QemuInterruptInfo);
	o_sm.write((const char*)&size_QemuInterruptInfo, 8);

	// sizeof(CPUState) (size_CPUSate) // 8 bytes
	uint64_t size_CPUSate = sizeof(CPUState);
	o_sm.write((const char*)&size_CPUSate, 8);

	// amount of TBs (amt_tbs)// 8 bytes
	uint64_t amt_tbs = rt_dump_tb_count;
	o_sm.write((const char*)&amt_tbs, 8);

	uint64_t amt_valid_interruptStates = 0;
	vector<uint8_t> is_valid;

	is_valid.reserve(amt_tbs);
	for(vector< interruptState_ty >::iterator it = m_interruptStates.begin();
			it != m_interruptStates.end(); ++it) {
		if( it->second != NULL ){
			is_valid.push_back(1);
			++amt_valid_interruptStates;
		} else {
			is_valid.push_back(0);
		}
	}
	assert(is_valid.size() == amt_tbs);

	// amount of non-empty interruptStates (amt_interruptStates)// 8 bytes
	o_sm.write((const char*)&amt_valid_interruptStates, 8);
	// flag to indicate whether a TB had a interrupt info (is_valid)// amt_tbs bytes
	o_sm.write((const char*)is_valid.data(), amt_tbs);

	// write data of all non-empty interruptState to file
	for(uint64_t i = 0; i < amt_tbs; ++i){
		if(is_valid[i] == 1){
			assert(m_interruptStates[i].second);
			o_sm.write((const char*)&m_interruptStates[i].first, size_QemuInterruptInfo);
			o_sm.write((const char*)m_interruptStates[i].second, size_CPUSate);
		} else {
			assert(!m_interruptStates[i].second);
		}
	}

	o_sm.clear();
}

/*
 * To verify the number of dumped TBs in RuntimeEnv is valid*/
void RuntimeEnv::verifyDumpData()
{
#if defined(CRETE_DEBUG) && defined(DBG_BO_CURRENT)
	cout << "[Verify dump data:] \n"
			<< dec << "rt_dump_tb_count = " << rt_dump_tb_count
			<< ", m_prolog_regs.size() = " << m_prolog_regs.size()
			<< ", m_memoSyncTables.size() = " << m_memoSyncTables.size()
			<< ", m_memoMergePoints.size() = " << m_memoMergePoints.size()
			<<", m_tbExecSequ.size() = " << m_tbExecSequ.size()
			<< ", m_interruptStates.size() = " << m_interruptStates.size() << endl;
#endif

	assert(m_prolog_regs.size() == rt_dump_tb_count &&
			"Something wrong in m_prolog_regs dump, its size should be equal to rt_dump_tb_count all the time.\n");
	assert(m_memoSyncTables.size() == rt_dump_tb_count &&
			"Something wrong in m_memoSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");
	assert(m_memoMergePoints.size() == rt_dump_tb_count &&
			"Something wrong in m_memoSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");
	assert(m_tbExecSequ.size() == rt_dump_tb_count &&
				"Something wrong in m_tbExecSequ dump, its size should be equal to rt_dump_tb_count all the time.\n");
	assert(m_interruptStates.size() == (rt_dump_tb_count - 1) &&
				"Something wrong in m_interruptStates dump, its size should be equal to (rt_dump_tb_count - 1) all the time.\n");
}
#endif

#if defined(HAVLICEK_TB_GRAPH)
void RuntimeEnv::addTBGraphInfo(TranslationBlock *tb)
{
    uint64_t current_pc = tb->pc;

    // Branches taken.
    if(crete_last_brcond_result.size() != 0 && m_tbExecSequInt.size() > 1)
    {
        uint64_t prev_tb_pc = m_tbExecSequInt.back();

        std::map<uint64_t, std::pair<bool, bool> >::iterator it =
                m_tbExecSequBranchResults.find(prev_tb_pc);

        if(it == m_tbExecSequBranchResults.end())
        {
            it = m_tbExecSequBranchResults.insert(make_pair(prev_tb_pc,
                                                            make_pair(false, false))).first;
        }

        bool branch = crete_last_brcond_result.top();
        crete_last_brcond_result.pop();

        if(branch)
        {
            it->second.first = true;
        }
        else
        {
            it->second.second = true;
        }
    }

    // Normal execution sequence.
    m_tbExecSequInt.push_back(current_pc);
}

void RuntimeEnv::writeTBAddresses()
{
    string path = getOutputFilename("tb-seq.bin");
    ofstream ofs(path.c_str(), ios_base::out | ios_base::binary);

    if(!ofs.good())
        throw runtime_error("can't open file: " + path);

    for(vector<uint64_t>::iterator iter = m_tbExecSequInt.begin();
        iter != m_tbExecSequInt.end();
        ++iter)
    {
        ofs.write(reinterpret_cast<const char*>(&*iter),
                  sizeof(uint64_t));
    }
}

void RuntimeEnv::writeNewTraceLog() // TODO: should this really be in under HAVLICEK_TB_GRAPH? Confused why I put it here.
{
    namespace fs = boost::filesystem;

    fs::path real_path(m_outputDirectory);

    if(!fs::exists(real_path))
        throw runtime_error("failed to find: " + real_path.generic_string());

    fs::path root(real_path.parent_path() / "new");
    if(!fs::exists(root))
        fs::create_directories(root);

    fs::path path = root / real_path.stem();
    if(!fs::exists(path))
        fs::create_directory_symlink(m_outputDirectory, path);
}

void RuntimeEnv::writeTBBranchResults()
{
    string path = getOutputFilename("tb-seq-br-results.bin");
    ofstream ofs(path.c_str(), ios_base::out | ios_base::binary);

    if(!ofs.good())
        throw runtime_error("can't open file: " + path);

    for(std::map<uint64_t, std::pair<bool, bool> >::iterator it =
            m_tbExecSequBranchResults.begin();
        it != m_tbExecSequBranchResults.end();
        ++it)
    {
        // PC value.
        ofs.write(reinterpret_cast<const char*>(&it->first),
                  sizeof(uint64_t));
        // Left branch.
        ofs.write(reinterpret_cast<const char*>(&it->second.first),
                  sizeof(bool));
        // Right branch.
        ofs.write(reinterpret_cast<const char*>(&it->second.second),
                  sizeof(bool));
    }
}

#endif // defined(HAVLICEK_TB_GRAPH)


void RuntimeEnv::dump_tlo_tb_pc(const uint64_t pc)
{
	m_tcg_llvm_offline_ctx.dump_tlo_tb_pc(pc);
}

void RuntimeEnv::dump_tcg_ctx(const TCGContext &tcg_ctx)
{
	m_tcg_llvm_offline_ctx.dump_tcg_ctx(tcg_ctx);
}

void RuntimeEnv::dump_tcg_temp(const vector<TCGTemp> &tcg_temp)
{
	m_tcg_llvm_offline_ctx.dump_tcg_temp(tcg_temp);
}

void RuntimeEnv::dump_tcg_helper_name(const TCGContext &tcg_ctx)
{
	m_tcg_llvm_offline_ctx.dump_tcg_helper_name(tcg_ctx);
}

void RuntimeEnv::dump_tlo_opc_buf(const uint64_t *opc_buf)
{
	m_tcg_llvm_offline_ctx.dump_tlo_opc_buf(opc_buf);
}

void RuntimeEnv::dump_tlo_opparam_buf(const uint64_t *opparam_buf)
{
	m_tcg_llvm_offline_ctx.dump_tlo_opparam_buf(opparam_buf);
}

void RuntimeEnv::writeTcgLlvmCtx()
{
	cout << "writeTcgLlvmCtx is invoked." << endl;

	string path = getOutputFilename("dump_tcg_llvm_offline.bin");

	ofstream ofs(path.c_str());

    boost::archive::binary_oarchive oa(ofs);
    oa << m_tcg_llvm_offline_ctx;

    cout << "writeTcgLlvmCtx is finished." << endl;
}

/*****************************/
/* Functions for QEMU c code */
void initialize_std_output()
{
#if defined(CRETE_DEBUG)
	printf("initialize_std_output is called.\n");
#endif // defined(CRETE_DEBUG)

#if defined(CRETE_DEBUG)
	ios_base::sync_with_stdio(true);
	cout.setf(ios_base::unitbuf);
	cerr.setf(ios_base::unitbuf);
#endif // defined(CRETE_DEBUG)
}

RuntimeEnv* runtime_dump_initialize()
{
	runtime_env = 0;
	rt_dump_tb = 0;
	rt_dump_tb_count = 0;
	flag_rt_dump_start = 0;
	flag_rt_dump_enable = 0;
	flag_getHostAddress = 0;
	flag_interested_tb = 0;
	flag_interested_tb_prev = 0;
	flag_memo_monitor_enable = 0;

#if defined(DBG_BO_CALL_STACK)
	flag_enable_monitor_call_stack = 1;
	flag_holdon_monitor_call_stack = 0;

	is_begin_capture = 0;
	is_target_pid = 0;
	is_user_code = 0;
#endif

#if defined(DBG_BO_REPLAY_INTERRUPT)
	flag_dump_interrupt_CPUState = 0;
#endif

    return new RuntimeEnv;
}

void runtime_dump_close(RuntimeEnv *rt)
{
    delete rt;
}

/* Main dump procedure for crete which will be called before the execution of every TB
 * */
void crete_runtime_dump(void *qemuCpuState, TranslationBlock *tb)
{
	CPUState *env = (CPUState *)qemuCpuState;

	// check assumptions we made by assertions
	assert(is_target_pid == (env->cr[3] == g_havlicek_target_pid) &&
			"env->cr[3] should stay the same within one iteration of inner loop of cpu_exec()\n");

	// set globals for debug using
	g_cpuState_bct = env;
	rt_dump_tb = tb;

//#if defined(CRETE_DEBUG)
	/* BOBO: xxx occasionally, flag_rt_dump_enable is not 0, which means the execution flow of qemu
	 *       was changed during the execution of "next_tb = tcg_qemu_tb_exec(env, tc_ptr);"
	 *       known situation: page_fault occurred and raise_exception_* has been invoked.
	 */
	if (flag_rt_dump_enable)
		printf("[Warning]: \"cpu-exec.c\" flag_rt_dump_enable = 1 before a TB is executed.\n");
//#endif // defined(CRETE_DEBUG)

	//1. Set flags related for runtime dump
	flag_rt_dump_enable = 0;
	flag_interested_tb_prev = flag_interested_tb;

	// set flags related to TB filter
	is_begin_capture = (g_custom_inst_emit == 1);
	is_target_pid = (env->cr[3] == g_havlicek_target_pid);
	is_user_code = (tb->pc < USER_CODE_RANGE);

	bool is_in_include_filter = crete_is_pc_in_include_filter_range(tb->pc);
	bool is_in_exclude_filter = crete_is_pc_in_exclude_filter_range(tb->pc);

	//2. Call stack monitor
#if defined(DBG_BO_CALL_STACK)
	bool is_in_callStack_limit = 0;
	bool is_processing_interrupt = 0;
	bool is_in_exclude_callStack = 0;

	is_processing_interrupt = runtime_env->isProcessingInterrupt();
	is_in_exclude_callStack = crete_is_pc_in_call_stack_exclude_filter_range(tb->pc);

	/* Call stack will work when all the following conditions are true:
	 * 1. capture begins, 2. it is in target process,
	 * 3. flag_enable_monitor_call_stack is true,
	 * 4. it is NOT processing interrupt
	 * 5. it is NOT in the exclude list of call stack, which contains:
	 * 		a> .plt
 	 * */
	bool is_working_call_stack = is_begin_capture && is_target_pid
			&& flag_enable_monitor_call_stack && !is_processing_interrupt
			&& !is_in_exclude_callStack;

	if(is_working_call_stack) {
		runtime_call_stack_update(runtime_env, tb->pc);

        if(runtime_call_stack_size(runtime_env) > 0) {
		is_in_callStack_limit = !flag_holdon_monitor_call_stack ?
                ( runtime_call_stack_size(runtime_env) <= g_crete_call_stack_bound) : 0;
        }
	}
#endif

	// 3. set flag of filter TB based on above flags
    int is_interested_tb =
            is_begin_capture &&
            is_target_pid &&
            is_user_code &&
            !is_in_exclude_filter &&
            (is_in_include_filter || is_in_callStack_limit) &&
            crete_flag_capture_enabled;

	// Set flags: flag_rt_dump_start/ flag_rt_dump_enable/ flag_interested_tb
	if(is_interested_tb)
	{
//		printf("dumped tb: TB-PC = 0x%x, call stack size = %d\n", tb->pc, runtime_call_stack_size(runtime_env));

		if(flag_rt_dump_start == 0)
		{
			flag_rt_dump_start = 1;
#if defined(CRETE_DEBUG)
			printf("1. Runtime dump start!\n");
#endif // defined(CRETE_DEBUG)
		}

		/* enable flg_rt_dump_enable */
		flag_rt_dump_enable = 1;

		flag_interested_tb = 1;
	} else {
		flag_interested_tb = 0;
	}

#if defined(DBG_BO_TCG_WORKFLOW)
	if(is_begin_capture && is_target_pid){
		printf("TB-PC = 0x%x\n", tb->pc);
		cerr << dec << ", is_in_exclude_filter = " << is_in_exclude_filter
				<<  ", is_in_include_filter = " << is_in_include_filter
				<< ", is_in_callStack_limit = " << is_in_callStack_limit
				<< ", is_interested_tb = " << is_interested_tb << endl;
	}
#endif

	// 4. Memory Monitor
	if(flag_interested_tb_prev == 0 && flag_interested_tb == 1) {
		add_memo_merge_point(runtime_env, BackToInterestTb);
	} else if(flag_interested_tb_prev == 1 && flag_interested_tb == 0) {
//		printf("outofInterestTb merge point is going to be added.\n");

		add_memo_merge_point(runtime_env, OutofInterestTb);
	} else if(flag_interested_tb_prev == 1 && flag_interested_tb == 1) {
		add_memo_merge_point(runtime_env, NormalTb);
	}

	// 5. Dump information about interrupt replay for previous interested TB
#if defined(DBG_BO_REPLAY_INTERRUPT)
	if (flag_dump_interrupt_CPUState) {
		// When there is an interrupt happened, the (updated) CPUState needs to be dumped and
		//it will be dumped right before the next first interested TB is executed
		if (flag_rt_dump_enable == 1) {
			dump_cpuState_for_interrupt(runtime_env, (void *)env);
			flag_dump_interrupt_CPUState = 0;
		}
	} else {
		// The interested TB that did not have an interrupt will be pushed an
		// empty record in m_interruptStates, for record purpose
		if (flag_interested_tb_prev) {
			add_empty_qemu_interrupt_state(runtime_env);
		}
	}
#endif

	/* 6. dump runtime information for current TB replay
	 * Including:
	 * 		1. CPUState (only for the first interested TB),
	 *    	2. Regs
	 *    	3. empty SyncTable for Memory Monitor
	 *    	4. llvm bitcode
	 *    	5. execution sequence */
	if(flag_rt_dump_enable) {
		++rt_dump_tb_count;

#if defined(CRETE_DEBUG)
//		printf("====================================\n"
//				"pc at dump: %x\n", tb->pc);
		printf("pc at dump: %x\n", tb->pc);
#endif // defined(CRETE_DEBUG)
		/* Only dump the CPUState for the first interested TB */
		if( get_llvm_tbCount(tcg_llvm_ctx) == 0) {
			dump_CPUState(runtime_env, (void*)env);
#if defined(CRETE_DEBUG)
			printf("2. CPUState has been dumped.\n");
#endif // defined(CRETE_DEBUG)
		}

		if(flag_interested_tb_prev == 0 && flag_interested_tb == 1) {
#if defined(CRETE_DEBUG)
			printf("2.1 valid prolog_regs has been dumped.\n");
#endif // defined(CRETE_DEBUG)
			dump_prolog_regs(runtime_env, (void *)env, 1);
		} else {
			dump_prolog_regs(runtime_env, (void *)env, 0);
		}

		add_memo_sync_table(runtime_env);

		if(tb->llvm_function == NULL){
#if defined(CRETE_DEBUG)
			printf("3. llvm bitcode has been generated.\n");
#endif // defined(CRETE_DEBUG)

			cpu_gen_llvm(env, tb);

#ifdef DBG_BO_CURRENT
			TCGContext *s = &tcg_ctx;
			tcg_func_start(s);
			gen_intermediate_code_pc(env, tb);
			dump_IR(s, tb->pc);
#endif
		}

		dump_TBExecSequ(runtime_env, tb);
#if defined(CRETE_DEBUG)
		printf("4. Start to execute %x, tb-count: %lu\n",
				rt_dump_tb->pc, (rt_dump_tb_count-1));
#endif // defined(CRETE_DEBUG)

		runtime_env->verifyDumpData();
	} // if(flag_rt_dump_enable)
}

void dump_CPUState(struct RuntimeEnv *rt, void *dumpCpuState)
{
	rt->addCpuStates(dumpCpuState);
}


void dump_prolog_regs(struct RuntimeEnv *rt, void *env_CpuState, int is_valid)
{
	rt->addPrologRegs(env_CpuState, is_valid);
}

void dump_TBExecSequ(struct RuntimeEnv *rt, TranslationBlock *tb)
{
	rt->addTBExecSequ(tb);
}

void dump_printInfo(struct RuntimeEnv *rt)
{
	rt->printInfo();
}

void dump_writeRtEnvToFile(struct RuntimeEnv *rt, const char* outputDirectory)
{
	rt->writeRtEnvToFile(outputDirectory ? outputDirectory  : "");
}

void add_memo_sync_table(struct RuntimeEnv *rt)
{
	rt->addMemoSyncTable();
}

void dump_memo_sync_table_entry(struct RuntimeEnv *rt, uint64_t addr, uint32_t size, uint64_t value)
{
	rt->addMemoSyncTableEntry(addr, size, value);
}

void add_memo_merge_point(struct RuntimeEnv *rt, enum MemoMergePoint_ty type_MMP)
{
	rt->addMemoMergePoint(type_MMP);
}

#if defined(DBG_BO_CALL_STACK)
void runtime_call_stack_update(struct RuntimeEnv *rt, uint64_t tb_pc)
{
	rt->updateCallStack(tb_pc);
}
int  runtime_call_stack_size(struct RuntimeEnv *rt)
{
	return rt->getCallStackSize();
}

void push_interrupt_stack(struct RuntimeEnv *rt, int intno, int is_int,
		int error_code, int next_eip, uint64_t interrupted_pc)
{
	QemuInterruptInfo interrup_info(intno, is_int, error_code, next_eip);
	rt->pushInterruptStack(interrup_info, interrupted_pc);
}

void pop_interrupt_stack(struct RuntimeEnv *rt, uint64_t resumed_pc)
{
	rt->popInterruptStack(resumed_pc);
}

#endif

#if defined(DBG_BO_REPLAY_INTERRUPT)
void add_qemu_interrupt_state(struct RuntimeEnv *rt,
		int intno, int is_int, int error_code, int next_eip_addend)
{
	QemuInterruptInfo interrup_info(intno, is_int, error_code, next_eip_addend);

	rt->addQemuInterruptState(interrup_info);
}

void add_empty_qemu_interrupt_state(struct RuntimeEnv *rt)
{
	rt->addEmptyQemuInterruptState();
}

void dump_cpuState_for_interrupt(struct RuntimeEnv *rt,void *dumpCpuState)
{
	rt->dumpCpuStateForInterrupt(dumpCpuState);
}
#endif

#if defined(HAVLICEK_TB_GRAPH)
void crete_brcond_stack_push(int b)
{
    crete_last_brcond_result.push(b);

    // As we pop this every TB execution, the size of this should never be more than 1.
    // If so, we're executing more than 1 brcond instruction per TB, or something else is amiss.
    // I'm using a stack (as opposed to a single variable) as a sanity-check.

    // Temporarily disable... as this is causing problems. What about interrupts? It could be interrupts causing this...
//    assert(crete_last_brcond_result.size() == 1);
}
#endif // defined(HAVLICEK_TB_GRAPH)

#if defined(CRETE_UNUSED_CODE) && 0
/* Dump TLB table
 * dumpCpuState: the CPU state that contains the TLB table that will be dumped
 * is_pf:
 * 		0 = indicate it was a common tlb miss in tlb_fill, in which tlb table will be dumped
 * 		1 = indicate it was a page fault in tlb_fill, in which a NULL pointer will be pushed
 * BOBO: xxx
 *      m_tlbTables[i] stores the pointer to the dumped TLB table for "i"th tlb_fill call at runtime
 *      which should also the the "i"th tlb_fill at each state of klee when the runtime trace is
 *      replayed off-line.
 */
void RuntimeEnv::addTlbTable(void *env_cpuState, int is_pf)
{
#if defined(CRETE_DEBUG)
	cerr << "addTlbTable is invoked. is_pf = " << is_pf << endl;
#endif // defined(CRETE_DEBUG)

	if(is_pf) {
		m_tlbTables.push_back(NULL);
		return;
	}

	assert(env_cpuState);

	uint64_t size_tlbTable= NB_MMU_MODES * CPU_TLB_SIZE * sizeof(CPUTLBEntry);
	uint8_t *new_tlbTable = new uint8_t [size_tlbTable];
	m_tlbTables.push_back((void *)new_tlbTable);

	CPUState *src_cpuState = (CPUState *) env_cpuState;
	void* src_tlbTable = (void *)src_cpuState->tlb_table;
	void* dst_tlbTable = m_tlbTables.back();

	memcpy(dst_tlbTable, src_tlbTable, size_tlbTable);

#if defined(CRETE_DEBUG)
	cout << hex << "env_cpuState = 0x" << env_cpuState
			<< ", src_cpuState = 0x" << src_tlbTable
			<< ", CPU_OFFSET(tlb_table) = 0x" << CPU_OFFSET(tlb_table)
			<< dec << endl;
#endif // defined(CRETE_DEBUG)

}

/* Dump TLB table for prolog usage:
 * When some un-interested TBs are executed before a interested TB is about to execute, the TLB table
 * might be updated in those un-interested TBs, which would affect the coming tlb lookaside. So we dump
 * the tlb table for every single interested TB that is the first interested TB after one or several
 * un-intersted TBs. Those dumped tlb tables will be updated to by prolog on klee to make sure to TLB
 * table state is consistant between online (qemu) and offline (klee)
 * dumpCpuState: the CPU state that contains the TLB table that will be dumped
 * is_valid:
 * 		0 = indicate tlb does not need to be dumped for this TB
 * 		1 = indicate tlb needs to be dumped for this TB
 * BOBO: m_prolog_tlbTables[i] stores the pointer to the dumped TLB table for "i"th llvm function that is called
 *      in "main_function.ll", which is all the "i"th intersted TB executed on qemu
 */
void RuntimeEnv::addPrologTlbTable(void *env_cpuState, int is_valid)
{
#if defined(CRETE_DEBUG)
	cerr << "[RuntimeEnv] addPrologTlbTable is invoked. is_valid = " << is_valid << endl;
#endif // defined(CRETE_DEBUG)

	if(!is_valid) {
		m_prolog_tlbTables.push_back(NULL);
		return;
	}

	assert(env_cpuState);

	uint64_t size_tlbTable= NB_MMU_MODES * CPU_TLB_SIZE * sizeof(CPUTLBEntry);
	uint8_t *new_tlbTable = new uint8_t [size_tlbTable];
	m_prolog_tlbTables.push_back((void *)new_tlbTable);

	CPUState *src_cpuState = (CPUState *) env_cpuState;
	void* src_tlbTable = (void *)src_cpuState->tlb_table;
	void* dst_tlbTable = m_prolog_tlbTables.back();

	memcpy(dst_tlbTable, src_tlbTable, size_tlbTable);
}

void RuntimeEnv::addEsp(void *env_cpuState)
{
    target_ulong esp  = -1;

    readCpuRegister(env_cpuState, &esp,
			CPU_OFFSET(regs[R_ESP]), sizeof(esp));

    assert(esp != -1);
    m_esps.push_back(esp);
}

void RuntimeEnv::addEbp(void *env_cpuState)
{
    target_ulong ebp  = -1;

    readCpuRegister(env_cpuState, &ebp,
			CPU_OFFSET(regs[R_EBP]), sizeof(ebp));

//    assert(ebp != -1);
    m_ebps.push_back(ebp);
}

void RuntimeEnv::setRodataInfo(uint64_t addr, uint64_t size)
{
    m_rodataInfo = make_pair(addr, size);
}

void RuntimeEnv::addElfMemo(const RuntimeEnv::ElfMemo& elf_memo)
{
    elf_memos.push_back(elf_memo);
}

/* "name value size, guest_address:host_address\n" */
// TODO: this method is unstable. We can't assume .rodata contains only c-strings. I can't, then, use '\0' as a delimiter.
// TODO: also unstable because I assume data is contiguous across guest-->host address space.
void RuntimeEnv::dumpRodata(void *env_cpuState)
{
#if defined(CRETE_DEBUG)
    cout << "dumping rodata" << endl;
#endif // defined(CRETE_DEBUG)

    struct RoData {
        RoData(uint64_t ga, uint64_t ha, uint64_t s) : guest_addr(ga), host_addr(ha), size(s) {}
        uint64_t guest_addr;
        uint64_t host_addr;
        uint64_t size;
    };

    uint64_t guest_addr = m_rodataInfo.first;
    uint64_t host_addr = RuntimeEnv::getHostAddress(env_cpuState, guest_addr, 1, 0);
    uint64_t size = m_rodataInfo.second;

    vector<RoData> entries;
    char* cb = (char*)host_addr;
    char* ce = cb;
    for(size_t i = 0; i < size; ++i)
    {
        if(ce[i] == '\0')
        {
            size_t dist = &ce[i] - cb;
            dist += 1; // + 1 for '\0'
            size_t dist_from_begin = &cb[0] - (char*)host_addr;
            entries.push_back(RoData(guest_addr + dist_from_begin, (uintptr_t)&cb[0], dist));
            cb = &ce[i + 1];
        }
    }

    for(vector<RoData>::iterator iter = entries.begin(); iter != entries.end(); ++iter)
    {
#if defined(CRETE_DEBUG)
        cout << string((char*)iter->host_addr, iter->size) << endl;
#endif // defined(CRETE_DEBUG)
    }

    for(vector<RoData>::iterator iter = entries.begin();
        iter != entries.end();
        ++iter)
    {
        size_t idx = distance(entries.begin(), iter);

        stringstream ss(ios_base::app | ios_base::out);
        ss << "rodata[" << idx << "]";
        ofstream ofs(getOutputFilename(ss.str()).c_str(), ios_base::out | ios_base::binary);

#if defined(CRETE_DEBUG)
        cout << "rodata outfile: " << getOutputFilename(ss.str()).c_str() << endl;
#endif // defined(CRETE_DEBUG)

        assert(ofs && "failed top open for writing rodata!");
        ofs.write(reinterpret_cast<char*>(iter->host_addr), iter->size);
        ss << ' ' << 0
           << ' ' << iter->size << ", "
           << iter->guest_addr << ':' << iter->host_addr
           << endl;

        m_concMemos.push_back(ss.str());

#if defined(CRETE_DEBUG)
        cout << ss.str();
#endif // defined(CRETE_DEBUG)

        /*uint32_t value_dump_unit_mo;
        uint64_t size_dump_unit_mo;
        uint64_t guest_dump_unit_mo;
        uint64_t host_dupm_unit_mo;

        const uint8_t *ptr_unit;
        size_dump_unit_mo = 1;

        for(uint32_t i = 0; i < size_rodata; ++i) {
            guest_dump_unit_mo = addr_rodata + i;
            host_dupm_unit_mo = getHostAddress(env_cpuState, guest_dump_unit_mo, 1, 0);
            ptr_unit = (uint8_t *)host_dupm_unit_mo;
            value_dump_unit_mo = *(ptr_unit);

            stringstream ss(ios_base::app | ios_base::out);
            ss << "rodata[" << i << "] " << value_dump_unit_mo << ' ' << size_dump_unit_mo << ", "
                    << guest_dump_unit_mo << ':' << host_dupm_unit_mo << '\n';

            cout << ss.str();
            m_concMemos.push_back(ss.str());
        }*/
    }
}

/*
 * For each MO:
 * Format:
 * address - 8 bytes
 * size - 4 bytes
 * data - "size" bytes
**/
void RuntimeEnv::dumpElfMemos()
{
    ofstream ofs(getOutputFilename("dump_elf.bin").c_str(), ios_base::out | ios_base::binary);
    for(vector<ElfMemo>::iterator iter = elf_memos.begin();
        iter != elf_memos.end();
        ++iter)
    {
        ofs.write(reinterpret_cast<const char*>(&(iter->addr)), sizeof(uint64_t));
        ofs.write(reinterpret_cast<const char*>(&(iter->size)), sizeof(uint32_t));
        copy(iter->data.begin(), iter->data.end(), ostream_iterator<uint8_t>(ofs));
    }
}

void RuntimeEnv::writeConcreteMemo()
{
	if(!m_concMemos.empty()) {
		ofstream o_concMos(getOutputFilename("dump_mo_concretes.txt").c_str());

#if defined(CRETE_DEBUG)
		cerr << "writeConcreteMemo():\n";
#endif // defined(CRETE_DEBUG)
		for(vector<string>::iterator it = m_concMemos.begin();
				it != m_concMemos.end(); ++it) {
			o_concMos << *it;
//			cerr << *it;
		}

		o_concMos.close();
	}

}


// write the information that is needed before a TB is replayed off-line
// such as: esp for each tb
void RuntimeEnv::writeTbPrologue()
{
	ofstream o_esps(getOutputFilename("dump_tbPrologue_esps.txt").c_str());
	assert(o_esps && "Can not create new file dump_tbPrologue_esps.txt.\n");

	assert(m_esps.size() == m_tbExecSequ.size());
	o_esps << "esp_size=" << sizeof(target_ulong) << '\n'
			<< "esp_offset=" << CPU_OFFSET(regs[R_ESP]) << '\n';
	for(int i = 0; i < m_esps.size(); ++i) {
		o_esps<< "esp-" << m_tbExecSequ[i] << ":" << m_esps[i] << '\n';
//		cerr<< "esp-" << m_tbExecSequ[i] << ":" << m_esps[i] << '\n';
	}

	o_esps.close();

	ofstream o_ebps(getOutputFilename("dump_tbPrologue_ebps.txt").c_str());
	assert(o_ebps && "Can not create new file dump_tbPrologue_ebps.txt.\n");

	assert(m_ebps.size() == m_tbExecSequ.size());
	o_ebps << "ebp_size=" << sizeof(target_ulong) << '\n'
			<< "ebp_offset=" << CPU_OFFSET(regs[R_EBP]) << '\n';
	for(int i = 0; i < m_ebps.size(); ++i) {
		o_ebps<< "ebp-" << m_tbExecSequ[i] << ":" << m_ebps[i] << '\n';
//		cerr<< "ebp-" << m_tbExecSequ[i] << ":" << m_ebps[i] << '\n';
	}

	o_ebps.close();
}

void RuntimeEnv::writeTlbTables()
{
    ofstream o_tlbs(getOutputFilename("dump_tlbs.txt").c_str());
	assert(o_tlbs && "Can not create new file dump_tlbs.txt.\n");

	uint64_t tlbTable_size= NB_MMU_MODES * CPU_TLB_SIZE * sizeof(CPUTLBEntry);
	uint64_t tlbTable_offset = CPU_OFFSET(tlb_table);

	o_tlbs << "tlbTable_size=" << tlbTable_size << '\n'
			<< "tlbTable_offset=" << tlbTable_offset << '\n'
			<< "tlbTable_amount=" << m_tlbTables.size() << '\n'
			<< "\ntlb_table_delimeter\n";

	char *src_ptr_tlb;
	for(int i = 0; i < m_tlbTables.size(); ++i) {
		src_ptr_tlb = (char *)m_tlbTables[i];

		if(src_ptr_tlb) {
			o_tlbs.write(src_ptr_tlb, tlbTable_size);
		} else {
			o_tlbs << "page_fault";
		}

		o_tlbs << "\ntlb_table_delimeter\n";
	}

    o_tlbs.close();;
}


void RuntimeEnv::writePrologTlbTables()
{
	assert(m_prolog_tlbTables.size() == m_tbExecSequ.size());

	ofstream o_tlbs(getOutputFilename("dump_tbPrologue_tlbs.txt").c_str());
	assert(o_tlbs && "Can not create new file dump_tlbs.txt.\n");

	uint64_t tlbTable_size= NB_MMU_MODES * CPU_TLB_SIZE * sizeof(CPUTLBEntry);
	uint64_t tlbTable_offset = CPU_OFFSET(tlb_table);

	o_tlbs << "tlbTable_size=" << tlbTable_size << '\n'
			<< "tlbTable_offset=" << tlbTable_offset << '\n'
			<< "tlbTable_amount=" << m_prolog_tlbTables.size() << '\n'
			<< "\ntlb_table_delimeter\n";

	char *src_ptr_tlb;
	for(int i = 0; i < m_prolog_tlbTables.size(); ++i) {
		src_ptr_tlb = (char *)m_prolog_tlbTables[i];

		if(src_ptr_tlb) {
			o_tlbs.write(src_ptr_tlb, tlbTable_size);
		} else {
			o_tlbs << "invlid_tlb_table";
		}

		o_tlbs << "\ntlb_table_delimeter\n";
	}

    o_tlbs.close();;

}

/*
 * Dump stack memories
 * env_cpuState: CPUState to read ESP and EBP
 * offset: the offset from ESP
 * size: in terms of Bytes
 */
void RuntimeEnv::addEspMemos(void *env_cpuState, uint32_t size, uint32_t offset)
{
//	Make sure the size could be divided by 4.
	if (size != -1)
		assert(size%4 == 0);

    target_ulong esp  = -1;
    target_ulong ebp  = -1;
    uint32_t dump_size;

#if defined(CRETE_DEBUG)
    cerr << "getHostAddress is invoked in addEspMemos:\n";
    cerr << "find host address for esp.\n";
#endif // defined(CRETE_DEBUG)

    readCpuRegister(env_cpuState, &esp,
			CPU_OFFSET(regs[R_ESP]), sizeof(esp));

    if (size == -1) {

#if defined(CRETE_DEBUG)
    cerr << "getHostAddress is invoked in addEspMemos:\n";
    cerr << "find host address for ebp.\n";
#endif // defined(CRETE_DEBUG)

    	readCpuRegister(env_cpuState, &ebp,
    			CPU_OFFSET(regs[R_EBP]), sizeof(ebp));
    	// Make sure ebp is not smaller than esp
    	assert(ebp >= esp);
    	// Make sure the size of the current frame could be divided by 4
    	assert((ebp - esp)%4 == 0);

        //BOBO:xxx dump 8 more bytes than the current frame of the stack
    	//			as the parameters are after EBP
    	dump_size = (ebp - esp)/4 + 2;
    } else
    	dump_size = size/4;

#if defined(CRETE_DEBUG)
    uint64_t host_esp = -1;
    uint64_t host_ebp = -1;
	host_esp = getHostAddress(env_cpuState, esp, 1);
	host_ebp = getHostAddress(env_cpuState, ebp, 1);

	cerr << "ESP: " << hex << esp << ":" << host_esp
				<< ", EBP: " << ebp << ":" <<host_ebp << dec << "\n";
#endif // defined(CRETE_DEBUG)

	target_ulong guest_dump_addr;
    uint64_t host_dump_addr;
    uint8_t *dump_ptr;

    uint32_t memo_value;
    uint32_t dump_memo_size = sizeof(memo_value);

    // The original assumption that was the host address for the stack is continuous is wrong.
    // And hence, it was fixed by computing every host address every 4 bytes.
    // BOBO: xxx this still seems not very safe either, because 4 bytes might not be continuous.
    for(int i = 0; i < dump_size; ++i) {
    	guest_dump_addr = esp + dump_memo_size*i;
    	host_dump_addr = getHostAddress(env_cpuState, guest_dump_addr, 1);
    	dump_ptr = (uint8_t *)host_dump_addr;

    	memcpy(&memo_value, dump_ptr, dump_memo_size);

        // TODO: permanently disable. This has no effect here.
         // TODO: temporarily disabled. Seeing if interfers with
        // Ensure concolic objects don't overlap the addresses of esp. If so, skip the esp.
//        if(overlaps_with_existing_mo(guest_dump_addr, dump_memo_size))
//            continue;

        // Format is:
        // name value size, guestAddress:HostAddress\n
        // argv 153829384 4, 3215970820:139838659736068\n
        ostringstream ss;
        ss << "esp[" << i << "] " << memo_value << " " << dump_memo_size
            << ", " << guest_dump_addr << ":" << host_dump_addr << '\n';
        m_concMemos.push_back(ss.str());
    }
}

void dump_TLBTable(struct RuntimeEnv *rt, void *env_CpuState, int is_pf)
{
	rt->addTlbTable(env_CpuState, is_pf);
}

void dump_prolog_TLBTable(struct RuntimeEnv *rt, void *env_CpuState, int is_valid)
{
	rt->addPrologTlbTable(env_CpuState, is_valid);
}

void dump_Esp(struct RuntimeEnv *rt, void *env_CpuState)
{
	rt->addEsp(env_CpuState);
}

void dump_Ebp(struct RuntimeEnv *rt, void *env_CpuState)
{
	rt->addEbp(env_CpuState);
}
#endif
