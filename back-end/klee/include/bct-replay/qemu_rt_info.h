#ifndef BCT_REPLAY_H
#define BCT_REPLAY_H

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

using namespace std;
namespace klee {
class ObjectState;
class Executor;
class ExecutionState;
class Expr;
}

class QemuRuntimeInfo;

extern QemuRuntimeInfo *g_qemu_rt_Info;

extern uint64_t g_test_case_count;

//FIXME: xxx
const uint64_t KLEE_ALLOC_RANGE_LOW  = 0x70000000;
const uint64_t KLEE_ALLOC_RANGE_HIGH = 0x7FFFFFFF;

struct QemuInterruptInfo {
	int m_intno;
	int m_is_int;
	int m_error_code;
	int m_next_eip_addend;

	QemuInterruptInfo(int intno, int is_int, int error_code, int next_eip_addend)
	:m_intno(intno), m_is_int(is_int),
	 m_error_code(error_code), m_next_eip_addend(next_eip_addend) {}
};

// Dumped data for replaying interrupt
// pair::first is the information of qemu interrupt
// pair::second is the CPUState when this interrupt completes

#if !defined(CRETE_QEMU10)
typedef pair<QemuInterruptInfo, bool> interruptState_ty;
#else
typedef pair<QemuInterruptInfo, vector<uint8_t> > interruptState_ty;
#endif

/*****************************/
/* Functions for klee */
QemuRuntimeInfo* qemu_rt_info_initialize();
void qemu_rt_info_cleanup(QemuRuntimeInfo *qrt);

/* Stores the information of Concolic variables, mainly get from reading file
 * "dump_mo_symbolics.txt" and "concrete_inputs.bin", and will be used to construct
 *  symbolic memories in KLEE's memory model.
 */
struct ConcolicVariable
{
    string m_name;
    vector<uint8_t> m_concrete_value;
    uint64_t m_data_size;
    uint64_t m_guest_addr;
    uint64_t m_host_addr;

	ConcolicVariable(string name, vector<uint8_t> concrete_value,
			uint64_t data_size, uint64_t guest_addr, uint64_t host_addr)
    :m_name(name),
     m_concrete_value(concrete_value),
     m_data_size(data_size),
     m_guest_addr(guest_addr),
     m_host_addr(host_addr) {}
};

typedef vector<ConcolicVariable *> concolics_ty;
typedef map<string, ConcolicVariable *> map_concolics_ty;
typedef pair<uint64_t, vector<uint8_t> > cv_concrete_ty; //(value, value_size)

// <offset from the beginning of CPUState, and its size >
typedef pair<uint64_t, uint64_t> cpuComponent_ty;

struct ConcreteMemoInfo
{
    uint64_t m_addr;
    uint32_t m_size;
    vector<uint8_t> m_data;

    ConcreteMemoInfo()
    :m_addr(0), m_size(0), m_data(vector<uint8_t>()) {}

	ConcreteMemoInfo(uint64_t addr, uint32_t size, vector<uint8_t> data)
	:m_addr(addr), m_size(size), m_data(data) {}
};

typedef vector<ConcreteMemoInfo> concreteMoInfo_ty;

typedef vector<vector<uint8_t> > prolog_regs_ty;

// addr is the key for this map
typedef map<uint64_t, ConcreteMemoInfo> memoSyncTable_ty;
typedef vector<memoSyncTable_ty> memoSyncTables_ty;

class QemuRuntimeInfo {
private:
	// The sequence of concolic variables here is the same as how they
	// were made as symbolic by calling crete_make_concolic in qemu
	concolics_ty m_concolics;
	map_concolics_ty m_map_concolics;

	// CPU standard registers dumped from QEMU for every TB
	cpuComponent_ty m_regs_ty;
	prolog_regs_ty m_prolog_regs;

	// Memory values dumped by memory monitor from QEMU
	memoSyncTables_ty m_memoSyncTables;

	// Interrupt State Info dumped from QEMU
	vector< interruptState_ty > m_interruptStates;

public:
	QemuRuntimeInfo();
	~QemuRuntimeInfo();

	void update_regs(klee::ExecutionState &state,
	        klee::ObjectState *wos, uint64_t tb_index);
	memoSyncTable_ty* get_memoSyncTable(uint64_t tb_index);
	void printMemoSyncTable(uint64_t tb_index);

	concolics_ty get_concolics() const;
	map_concolics_ty get_mapConcolics() const;

	QemuInterruptInfo get_qemuInterruptInfo(uint64_t tb_index);
	void update_qemu_CPUState(klee::ObjectState *wos,
			uint64_t tb_index)
	__attribute__ ((deprecated));

private:
	void init_prolog_regs();

	//TODO: xxx not a good solution
	void check_file_symbolics();

	void init_concolics();
	void cleanup_concolics();

	void init_memoSyncTables();
	void print_memoSyncTables();

	void init_interruptStates();
};
#endif
