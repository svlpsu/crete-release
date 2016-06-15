#include "bct-replay/qemu_rt_info.h"
#include "../Core/Memory.h"
#include "klee/ExecutionState.h"
#include "crete/test_case.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <iomanip>

QemuRuntimeInfo *g_qemu_rt_Info = 0;

QemuRuntimeInfo::QemuRuntimeInfo()
{
	init_prolog_regs();
	init_memoSyncTables();
	assert(m_prolog_regs.size() == m_memoSyncTables.size());

	init_concolics();
	init_interruptStates();
}

QemuRuntimeInfo::~QemuRuntimeInfo()
{
    cleanup_concolics();
}

static void check_and_update_regs(klee::ExecutionState& state,
        klee::ObjectState *wos_tcg_env, uint64_t regs_offset,
        vector<uint8_t> &regs_value, uint64_t tb_index)
{
    uint64_t reg_num = 0; // the number of registers
    uint64_t reg_size = 0; // the size of each register

    //FIXIT: xxx a hack to get the information of cpu architecture
    if (regs_value.size() == 32) {
        // 1: 32 bits, 8 regs of 4 bytes
        reg_num = 8;
        reg_size = 4;
    } else if (regs_value.size() == 128){
        // 2: 64 bits, 16 regs of 8 bytes
        reg_num = 16;
        reg_size = 8;
    } else {
        assert(0 && "regs size should be either 32 bytes (32-bits cpu) "
                "or 128 bytes (64-bits cpu)");
    }

    // check side effects for each register
    for(uint32_t i = 0; i < reg_num; ++i) {
        bool side_effect = false;
        uint64_t offset_within_regs = 0; // offset from the first bit of regs

        klee::ref<klee::Expr> ref_current_value_byte;
        uint8_t current_value_byte;
        for(uint32_t j = 0; j < reg_size; ++j){
            offset_within_regs = i*reg_size + j;
            assert(offset_within_regs < regs_value.size());

            ref_current_value_byte = wos_tcg_env->read8(regs_offset + offset_within_regs);

            if(!isa<klee::ConstantExpr>(ref_current_value_byte)) {
                ref_current_value_byte = state.getConcreteExpr(ref_current_value_byte);
                assert(isa<klee::ConstantExpr>(ref_current_value_byte));
            }

            current_value_byte = (uint8_t)llvm::cast<klee::ConstantExpr>(
                    ref_current_value_byte)->getZExtValue(8);
            if(current_value_byte != regs_value[offset_within_regs]) {
                side_effect = true;
                break;
            }
        }

        //update the value of the current register if there is a side-effect
        if(side_effect) {
            for(uint32_t j = 0; j < reg_size; ++j){
                offset_within_regs = i*reg_size + j;
                wos_tcg_env->write8(regs_offset + offset_within_regs,
                        regs_value[offset_within_regs]);
            }
        }
    }
}

void QemuRuntimeInfo::update_regs(klee::ExecutionState &state,
        klee::ObjectState *wos, uint64_t tb_index)
{
	uint64_t regs_offset = m_regs_ty.first;
	uint64_t regs_size = m_regs_ty.second;
	vector<uint8_t> regs_value = m_prolog_regs[tb_index];

	if(!regs_value.empty()) {
	    assert(regs_value.size() == regs_size);
	    check_and_update_regs(state, wos, regs_offset,
	            regs_value, tb_index);
	}
}

memoSyncTable_ty* QemuRuntimeInfo::get_memoSyncTable(uint64_t tb_index)
{
	assert(tb_index < m_memoSyncTables.size());
	return &m_memoSyncTables[tb_index];
}

void QemuRuntimeInfo::printMemoSyncTable(uint64_t tb_index)
{
	memoSyncTable_ty temp_mst = m_memoSyncTables[tb_index];

	cerr << "memoSyncTable content of index " << dec << tb_index << ": ";

	if(temp_mst.empty()){
		cerr << " NULL\n";
	} else {
		cerr << "size = " << temp_mst.size() << ":\n";
		for(memoSyncTable_ty::iterator m_it = temp_mst.begin();
				m_it != temp_mst.end(); ++m_it){

			cerr << hex << "0x" << m_it->first << ": (0x " << m_it->second.m_size <<", 0x ";

			for(vector<uint8_t>::iterator uint8_it =  m_it->second.m_data.begin();
					uint8_it !=  m_it->second.m_data.end(); ++uint8_it){
				cerr << (uint32_t)(*uint8_it) << " ";
			}
			cerr << ")" << "; ";
		}

		cerr << dec << endl;
	}
}

concolics_ty QemuRuntimeInfo::get_concolics() const
{
	return m_concolics;
}

map_concolics_ty QemuRuntimeInfo::get_mapConcolics() const
{
	return m_map_concolics;
}

/* The format of "dump_tbPrologue_regs.bin" is:
 * size_regs_entry // 8bytes
 * amount of TBs (amt_tbs)// 8 bytes
 * flag to indicate whether a TB needs to update regs // amt_tb bytes
 * data of dumped regs // (amount of TB needs to update regs) * size_regs_entry bytes
 * */
void QemuRuntimeInfo::init_prolog_regs()
{
	ifstream i_regs("dump_tbPrologue_regs.bin", ios_base::binary);
	assert(i_regs && "Fail to open file: dump_tbPrologue_regs.bin\n");

	uint64_t offset_regs;
	i_regs.read((char *)&offset_regs, 8);
	assert(i_regs.gcount() == 8);

	uint64_t size_regs_entry;
	i_regs.read((char *)&size_regs_entry, 8);
	assert(i_regs.gcount() == 8);

	m_regs_ty = make_pair(offset_regs, size_regs_entry);

	uint64_t amt_regs;
	i_regs.read((char *)&amt_regs, 8);
	assert(i_regs.gcount() == 8);

	vector<uint8_t> is_valid(amt_regs);
	i_regs.read((char *)is_valid.data(), amt_regs);
	assert((uint64_t)i_regs.gcount() == amt_regs);

	vector<uint8_t> regs_entry(size_regs_entry);
	for(vector<uint8_t>::iterator it = is_valid.begin();
			it != is_valid.end(); ++it) {
		if(*it == 1) {
			i_regs.read((char *)regs_entry.data(), size_regs_entry);
			assert((uint64_t)i_regs.gcount() == size_regs_entry);
			m_prolog_regs.push_back(regs_entry);
		} else {
			assert(*it == 0);
			m_prolog_regs.push_back(vector<uint8_t>());
		}
	}

	i_regs.clear();
}

void QemuRuntimeInfo::check_file_symbolics()
{
    ifstream ifs("dump_mo_symbolics.txt", ios_base::binary);
    vector<string> symbolics;
    string symbolic_entry;
    while(getline(ifs, symbolic_entry, '\n')) {
    	symbolics.push_back(symbolic_entry);
    }

    ifs.close();

    set<string> unique_symbolics;
    vector<string> output_symbolics;
    for(vector<string>::iterator i = symbolics.begin();
    		i != symbolics.end(); ++i) {
    	if(unique_symbolics.insert(*i).second){
    		output_symbolics.push_back(*i);
    	}
    }

    ofstream ofs("dump_mo_symbolics.txt", ios_base::binary);
    for(vector<string>::iterator i = output_symbolics.begin();
        		i != output_symbolics.end(); ++i) {
    	ofs << *i << '\n';
    }
    ofs.close();
}

//Get the information of concolic variables from file "dump_mo_symbolics" and "concrete_inputs.bin"
void QemuRuntimeInfo::init_concolics()
{
    using namespace crete;

    check_file_symbolics();

    ifstream inputs("concrete_inputs.bin", ios_base::in | ios_base::binary);
    assert(inputs && "failed to open concrete_inputs file!");
    TestCase tc = read_test_case(inputs);

    // Get the concrete value of conoclic variables and put them in a map indexed by name
    vector<TestCaseElement> tc_elements = tc.get_elements();
    map<string, cv_concrete_ty> map_concrete_value;
    for(vector<TestCaseElement>::iterator it = tc_elements.begin();
    		it != tc_elements.end(); ++it) {
    	string c_name(it->name.begin(), it->name.end());
    	cv_concrete_ty pair_concrete_value(it->data_size,
    			it->data);
    	map_concrete_value.insert(pair<string, cv_concrete_ty>(c_name,
    			pair_concrete_value));
    }
    assert(map_concrete_value.size() == tc_elements.size());

    ifstream ifs("dump_mo_symbolics.txt");
    assert(ifs && "failed to open dump_mo_symbolics file!");

    string name;
    vector<uint8_t> concrete_value;
    uint64_t data_size;
    uint64_t guest_addr;
    uint64_t host_addr;

    uint64_t name_addr;
    uint64_t fake_val;

    map<string, cv_concrete_ty>::iterator map_it;
    ConcolicVariable *cv;
    string line;

    while(getline(ifs, line)) {
        stringstream sym_ss(line);
        sym_ss >> name;
        sym_ss >> name_addr;
        sym_ss >> fake_val;
        sym_ss >> data_size;
        sym_ss >> guest_addr;
        sym_ss >> host_addr;

        map_it = map_concrete_value.find(name);
        assert(map_it != map_concrete_value.end() &&
        		"concrete value for a concolic variable is not found!\n");

        concrete_value = map_it->second.second;
        data_size= map_it->second.first;

        cv = new ConcolicVariable(name, concrete_value, data_size,
        		guest_addr, host_addr);
        m_concolics.push_back(cv);

        m_map_concolics.insert(pair<string, ConcolicVariable *> (name, cv));
    }

}

void QemuRuntimeInfo::cleanup_concolics()
{
	while(!m_concolics.empty()){
		ConcolicVariable *ptr_cv = m_concolics.back();
		m_concolics.pop_back();
		delete ptr_cv;
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
void QemuRuntimeInfo::init_memoSyncTables()
{
	ifstream i_sm("dump_sync_memos.bin", ios_base::binary);
	assert(i_sm && "open file failed: dump_sync_memos.bin\n");

	// - amount of dumped TBs (amt_dumped_tbs)// 8 bytes
	uint64_t amt_dumped_tbs = 0;
	i_sm.read((char*)&amt_dumped_tbs, sizeof(amt_dumped_tbs));
	assert(i_sm.gcount() == 8);

	// - flag to indicate whether a TB needs to do MemoSync (flags_need_memo_sync)// amt_dumped_tbs bytes
    vector<uint8_t> flags_need_memo_sync(amt_dumped_tbs);
    i_sm.read((char*)flags_need_memo_sync.data(), amt_dumped_tbs);
	assert((uint64_t)i_sm.gcount() == amt_dumped_tbs);
	assert(flags_need_memo_sync.size() == amt_dumped_tbs);

	// - amount of non-empty memoSyncTable (amt_memoSyncTables)// 8 bytes
	uint64_t amt_memoSyncTables = 0;
	i_sm.read((char*)&amt_memoSyncTables, sizeof(amt_memoSyncTables));
	assert(i_sm.gcount() == 8);

	// read data of all non-empty memoSyncTables from file
	for(vector<uint8_t>::iterator it = flags_need_memo_sync.begin();
			it != flags_need_memo_sync.end(); ++it) {
		memoSyncTable_ty empty_memoSyncTable;

		if(*it == 1) {
			assert((amt_memoSyncTables--) != 0);

			uint64_t amt_memo_entries = 0;
			// - amount of ConcreteMemoInfo entries (amt_memo_entries) // 8 bytes
			i_sm.read((char*)&amt_memo_entries, sizeof(amt_memo_entries));
			assert(i_sm.gcount() == 8);

			for(uint64_t i = 0; i < amt_memo_entries; ++i){
				// - address (addr_memo_sync)// 8bytes
				uint64_t addr_memo_sync = 0;
				i_sm.read((char*)&addr_memo_sync, sizeof(addr_memo_sync));
				assert((uint64_t)i_sm.gcount() == 8);

				// - data_size (size_memo_sync)// 4 bytes
				uint32_t size_memo_sync = 0;
				i_sm.read((char*)&size_memo_sync, sizeof(size_memo_sync));
				assert((uint64_t)i_sm.gcount() == 4);
				assert(size_memo_sync != 0);

				// - data (data_memo_sync)// data_size bytes
				vector<uint8_t> data_memo_sync(size_memo_sync);
				i_sm.read((char*)data_memo_sync.data(), data_memo_sync.size()*sizeof(uint8_t));
				assert((uint64_t)i_sm.gcount() == size_memo_sync);

				ConcreteMemoInfo conc_memo_info(addr_memo_sync, size_memo_sync, data_memo_sync);
				empty_memoSyncTable.insert(pair<uint64_t, ConcreteMemoInfo>(addr_memo_sync, conc_memo_info));
			}
		} else {
			assert(*it == 0);
			assert(empty_memoSyncTable.size() == 0);
		}

		m_memoSyncTables.push_back(empty_memoSyncTable);
	}

	assert(amt_memoSyncTables == 0 &&
			"Reading file error: the amount of non-empty memoSyncTable is not matched.\n");

	i_sm.close();

	CRETE_DBG(print_memoSyncTables(););
}

void QemuRuntimeInfo::print_memoSyncTables()
{
	uint64_t temp_tb_count = 0;
	cerr << "[Memo Sync Table]\n";
	for(memoSyncTables_ty::iterator it = m_memoSyncTables.begin();
			it != m_memoSyncTables.end(); ++it){
		if(it->empty()){
			cerr << "tb_count: " << dec << temp_tb_count++ << ": NULL\n";
		} else {
			cerr << "tb_count: " << dec << temp_tb_count++
					<< ", size = " << it->size() << ": ";
			for(memoSyncTable_ty::iterator m_it = it->begin();
					m_it != it->end(); ++m_it){
				cerr << hex << "0x" << m_it->first << ": (0x " << m_it->second.m_size <<", 0x ";

				for(vector<uint8_t>::iterator uint8_it =  m_it->second.m_data.begin();
						uint8_it !=  m_it->second.m_data.end(); ++uint8_it){
					cerr << (uint32_t)(*uint8_it) << " ";
				}
				cerr << ")" << "; ";
			}

			cerr << dec << endl;
		}
	}
}
#if !defined(CRETE_QEMU10)
/* The format of "dump_qemu_interrupt_info.bin" is:
 * sizeof(QemuInterruptInfo) (size_QemuInterruptInfo) // 8 bytes
 * amount of TBs (amt_tbs)// 8 bytes
 * amount of non-empty interruptStates (amt_interruptStates)// 8 bytes
 * flag to indicate whether a TB had an interrupt state info (is_valid)// amt_tbs bytes
 * data of dumped nonempty interruptState_ty
 * - format of each nonempty entry of interruptState_ty:
 *   data of size_QemuInterruptInfo // size_QemuInterruptInfo bytes
 *   data of CPUState // size_CPUSate bytes
 * */
void QemuRuntimeInfo::init_interruptStates()
{
	ifstream i_sm("dump_qemu_interrupt_info.bin", ios_base::binary);
	assert(i_sm && "open file failed: dump_qemu_interrupt_info.bin\n");

	// sizeof(QemuInterruptInfo) (size_QemuInterruptInfo) // 8 bytes
	uint64_t size_QemuInterruptInfo = 0;
	i_sm.read((char*)&size_QemuInterruptInfo, 8);
	assert(i_sm.gcount() == 8);
	assert(size_QemuInterruptInfo == sizeof(QemuInterruptInfo));

	// amount of TBs (amt_tbs)// 8 bytes
	uint64_t amt_tbs = 0;
	i_sm.read((char*)&amt_tbs, 8);
	assert(i_sm.gcount() == 8);

	// amount of non-empty interruptStates (amt_interruptStates)// 8 bytes
	uint64_t amt_valid_interruptStates = 0;
	i_sm.read((char*)&amt_valid_interruptStates, 8);
	assert(i_sm.gcount() == 8);

	// flag to indicate whether a TB had a interrupt info (is_valid)// amt_tbs bytes
    vector<uint8_t> is_valid(amt_tbs);
    i_sm.read((char*)is_valid.data(), amt_tbs);
	assert((uint64_t)i_sm.gcount() == amt_tbs);
	assert(is_valid.size() == amt_tbs);

	// read data of all non-empty interruptState from file
	QemuInterruptInfo temp_qemu_interrupt_info(0, 0, 0, 0);
	for(vector<uint8_t>::iterator it = is_valid.begin();
			it != is_valid.end(); ++it) {

		if(*it == 1) {
			assert((amt_valid_interruptStates--) != 0);

			i_sm.read((char *)&temp_qemu_interrupt_info, size_QemuInterruptInfo);
			assert((uint64_t)i_sm.gcount() == size_QemuInterruptInfo);

			m_interruptStates.push_back(make_pair(temp_qemu_interrupt_info, true));
		} else {
			assert(*it == 0);
			QemuInterruptInfo blank_qemu_interrupt_info(0, 0, 0, 0);

			m_interruptStates.push_back(make_pair(blank_qemu_interrupt_info, false));
		}
	}

	assert(amt_valid_interruptStates == 0 &&
			"Reading file error: the amount of non-empty interruptState is not matched.\n");

	i_sm.close();
}
#else
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
void QemuRuntimeInfo::init_interruptStates()
{
	ifstream i_sm("dump_qemu_interrupt_info.bin", ios_base::binary);
	assert(i_sm && "open file failed: dump_qemu_interrupt_info.bin\n");

	// sizeof(QemuInterruptInfo) (size_QemuInterruptInfo) // 8 bytes
	uint64_t size_QemuInterruptInfo = 0;
	i_sm.read((char*)&size_QemuInterruptInfo, 8);
	assert(i_sm.gcount() == 8);
	assert(size_QemuInterruptInfo == sizeof(QemuInterruptInfo));

	// sizeof(CPUState) (size_CPUSate) // 8 bytes
	uint64_t size_CPUSate = 0;
	i_sm.read((char*)&size_CPUSate, 8);
	assert(i_sm.gcount() == 8);

	// amount of TBs (amt_tbs)// 8 bytes
	uint64_t amt_tbs = 0;
	i_sm.read((char*)&amt_tbs, 8);
	assert(i_sm.gcount() == 8);

	// amount of non-empty interruptStates (amt_interruptStates)// 8 bytes
	uint64_t amt_valid_interruptStates = 0;
	i_sm.read((char*)&amt_valid_interruptStates, 8);
	assert(i_sm.gcount() == 8);

	// flag to indicate whether a TB had a interrupt info (is_valid)// amt_tbs bytes
    vector<uint8_t> is_valid(amt_tbs);
    i_sm.read((char*)is_valid.data(), amt_tbs);
	assert((uint64_t)i_sm.gcount() == amt_tbs);
	assert(is_valid.size() == amt_tbs);

	// read data of all non-empty interruptState to file
	QemuInterruptInfo temp_qemu_interrupt_info(0, 0, 0, 0);
	vector<uint8_t> temp_CPUState(size_CPUSate);
	for(vector<uint8_t>::iterator it = is_valid.begin();
			it != is_valid.end(); ++it) {

		if(*it == 1) {
			assert((amt_valid_interruptStates--) != 0);

			i_sm.read((char *)&temp_qemu_interrupt_info, size_QemuInterruptInfo);
			assert((uint64_t)i_sm.gcount() == size_QemuInterruptInfo);

			i_sm.read((char *)temp_CPUState.data(), size_CPUSate);
			assert((uint64_t)i_sm.gcount() == size_CPUSate);

			m_interruptStates.push_back(make_pair(temp_qemu_interrupt_info, temp_CPUState));
		} else {
			assert(*it == 0);
			QemuInterruptInfo blank_qemu_interrupt_info(0, 0, 0, 0);

			m_interruptStates.push_back(make_pair(blank_qemu_interrupt_info, vector<uint8_t>()));
		}
	}

	assert(amt_valid_interruptStates == 0 &&
			"Reading file error: the amount of non-empty interruptState is not matched.\n");

	i_sm.close();
}
#endif


QemuInterruptInfo QemuRuntimeInfo::get_qemuInterruptInfo(uint64_t tb_index)
{
	return m_interruptStates[tb_index].first;
}

void QemuRuntimeInfo::update_qemu_CPUState(klee::ObjectState *wos, uint64_t tb_index)
{
	assert(0);
}

/*****************************/
/* Functions for klee */
QemuRuntimeInfo* qemu_rt_info_initialize()
{
	return new QemuRuntimeInfo;
}

void qemu_rt_info_cleanup(QemuRuntimeInfo *qrt)
{
	delete qrt;
}
