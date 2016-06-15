#include "custom-instructions.h"
#include <boost/serialization/split_member.hpp>
#include <string>
#include <stdlib.h>

extern "C" {
#include "tcg-op.h"
#include <qemu-timer.h>
#include "cpu.h"
#include "monitor.h"
#include "c-wrapper.h"
}

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#include "runtime-dump.h"
#include <tcg-llvm.h>
#include <crete/custom_opcode.h>
#include <crete/debug_flags.h>
#include <boost/system/system_error.hpp>

#if defined(HAVLICEK_INPUT) || 1
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem/operations.hpp>
#endif // HAVLICEK_INPUT || 1

extern RuntimeEnv *runtime_env;
#define CPU_OFFSET(field) offsetof(CPUState, field)

extern "C" {
extern CPUState *g_cpuState_bct;

//extern target_ulong g_havlicek_target_pid;
//extern int g_custom_inst_emit;
int crete_is_include_filter_empty(void);

//BOBO: touched for code refactoring of crete_runtime_dump
int crete_is_pc_in_exclude_filter_range(uint64_t pc);
int crete_is_pc_in_include_filter_range(uint64_t pc);

#if defined(DBG_BO_CALL_STACK)
int crete_is_pc_in_call_stack_exclude_filter_range(uint64_t pc);
#endif

extern int flag_rt_dump_enable;

}

std::string crete_data_dir;

bool g_havlicek_tmp_workaround = false; // TODO temporary workaround for bug
static bool crete_flag_write_initial_input = false;

class PCFilter
{
public:
    PCFilter(target_ulong addr_start, target_ulong addr_end) : addr_start_(addr_start), addr_end_(addr_end) {}
    bool is_in_range(target_ulong pc) { return pc >= addr_start_ && pc < addr_end_; } // Must be < addr_end_, not <= addr_end_.
private:
    target_ulong addr_start_, addr_end_;
};

static vector<PCFilter> g_pc_exclude_filters;
static vector<PCFilter> g_pc_include_filters;

#if defined(DBG_BO_CALL_STACK)
static vector<PCFilter> g_pc_call_stack_exclude_filters;
#endif

std::string crete_find_file(CreteFileType type, const char *name)
{
    namespace fs = boost::filesystem;

    fs::path fpath = crete_data_dir;

    switch(type)
    {
    case CRETE_FILE_TYPE_LLVM_LIB:
//#if defined(TARGET_X86_64)
//        fpath /= "../x86_64-softmmu/";
//#else
//        fpath /= "../i386-softmmu/";
//#endif // defined(TARGET_X86_64)
        break;
    case CRETE_FILE_TYPE_LLVM_TEMPLATE:
        //fpath /= "../runtime-dump/";
        break;
    }

    fpath /= name;

    if(!fs::exists(fpath))
    {
        throw std::runtime_error("failed to find file: " + fpath.string());
    }

    return fpath.string();
}

static void reset_llvm()
{
    // Reacquire
    tcg_llvm_ctx = new TCGLLVMContext; // Don't call tcg_llvm_initialize, as it should only be called once (upon program init).
    assert(tcg_llvm_ctx);

#if defined(DBG_TCG_LLVM_OFFLINE)
    string libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-1.0-op-helper-i386.bc");
    tcg_linkWithLibrary(tcg_llvm_ctx, libraryName.c_str());

    libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-1.0-crete-helper.bc");
    tcg_linkWithLibrary(tcg_llvm_ctx, libraryName.c_str());

    tcg_llvm_initHelper(tcg_llvm_ctx);
#endif
}

static void dump_guest_memory(uint64_t msg_addr) {
	stringstream ss;
	string token;
	vector<string> str_parsed;
	RuntimeEnv::DumpMemoType memo_type;

	ss << (char *) msg_addr;

	getline(ss, token, '\n');
	if (token.compare(15, 9, "Symbolic.") == 0)
		memo_type = RuntimeEnv::SymbolicMemo;
	else if (token.compare(15, 9, "Concrete.") == 0)
		memo_type = RuntimeEnv::ConcreteMemo;
	else {
        throw runtime_error("incorrect guest message: " + ss.str());

//		cerr << "[Error]: incorrect guest message:\n"
//				<< ss.str() << endl;
//		assert(0 && "[Error]: incorrect guest message.\n");
	}

	// Each token's format is:
	// "name value size, guest_address;"
	while (getline(ss, token, ';')) {
		str_parsed.push_back(token);
	}

	// Get the guest address and convert it host address,
	// then store it into runtime_env, its format is:
	// "name value size, guest_address:host_address\n"
	uint64_t guest_address;
	uint64_t host_address;
	for (vector<string>::iterator it = str_parsed.begin();
			it != str_parsed.end(); ++it) {
		stringstream ss_1(*it);

		getline(ss_1, token, ','); // Get the "name value size"
		getline(ss_1, token, ','); // Get the guest_address
		stringstream s2i_convert(token); // Convert string to integer
		s2i_convert >> guest_address;

#ifdef DBG_BO_CURRENT
#if defined(CRETE_DEBUG)
		cerr << "getHostAddress is invoked in dump_guest_memory."
				<< "guest address is: " << hex << guest_address << endl;
		cerr << "g_cpuState_bct: " << (uint64_t)g_cpuState_bct
    			<< "env from op_helper: "
    			<< dec << endl;
#endif // defined(CRETE_DEBUG)
#endif
#if defined(CRETE_DEBUG)
		cerr << "[test];" ;
#endif // defined(CRETE_DEBUG)

        // We don't know whether the "tee bug" exists in x64,
        // and which address range would indicate it, so we
        // only do the check on I386 for now.
#if !defined(TARGET_X86_64)
		//BOBO: xxx a quick hack here to filter the apparently incorrect guest address
        if(guest_address <= 0x8048000) {
#if defined(CRETE_DEBUG)
            cerr << "[tee bug] happened and bypassed!\n";
#endif // defined(CRETE_DEBUG)

            stringstream ss;
            ss << "invalid guest address";
            ss << guest_address;
            throw runtime_error(ss.str());
        }
        assert(guest_address > 0x8048000 && "invalid guest address\n");
#endif // !defined(TARGET_X86_64)

		host_address = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_address, 1);

		ostringstream ss_o(*it, ios_base::app);
		ss_o << ":" << host_address << '\n';
		runtime_env->addMemoStr(ss_o.str(), memo_type);
	}
}

static void feed_test_case(uint64_t msg_addr) {
	//======================
	ifstream i_myfile("hostfile//input_arguments.bin");

	if (i_myfile) {
		// To feed new test case to guest os by chaning the vlue of argv[i]
		stringstream tc_ss;
		vector<string> tc_str_parsed;
		string tc_token;

		tc_ss << (char *) msg_addr;

		// Make sure the message got is for feeding test case
		getline(tc_ss, tc_token, '\n');
		assert(tc_token.compare(0, 21, "Feed test case start.") == 0);

		string init_name;
		string init_size;
		uint64_t tc_guest_address;
		uint64_t tc_host_address;

		string input_name;
		string input_size;
		string input_value;

		// Get the content in the format of "name size guest_address value"
		// As the value is a string and hence '\0' is used to be the delimiter.
		// getline(tc_ss, tc_token, '\0');
		getline(tc_ss, init_name, ' '); // get "name" in the format of "argv[i]"

		getline(tc_ss, init_size, ' '); // get "size"

		getline(tc_ss, tc_token, ' '); // Get the guest_address
		stringstream s2i_convert(tc_token); // Convert string to integer
		s2i_convert >> tc_guest_address;

#if !defined(TARGET_X86_64)
		//BOBO: xxx a quick hack here to filter the apparently incorrect guest address
        if(tc_guest_address <= 0x8048000) {
#if defined(CRETE_DEBUG)
        	cerr << "[tee bug] happened and bypassed!\n";
#endif // defined(CRETE_DEBUG)

            stringstream ss;
            ss << "invalid guest address";
            ss << tc_guest_address;
            throw runtime_error(ss.str());
        }
        assert(tc_guest_address > 0x8048000 && "invalid guest address in feed_test_case.\n");
#endif // !defined(TARGET_X86_64)

		getline(tc_ss, tc_token, '\0'); // Get init_value of argv[i]
#if defined(CRETE_DEBUG)
		cerr << "Feed test case start in qemu.\n" << "Original " << init_name
				<< " passed from guest is: \n" << tc_token << endl;
#endif // defined(CRETE_DEBUG)

		// Get the test case from file "input_arguments.bin"
		// First, get the index of argv that will be replaced
		uint32_t argv_idx = 0;
		uint32_t i = 5;

		assert(init_name.compare(0, 5, "argv[") == 0);
		while (init_name.at(i) != ']') {
			argv_idx = argv_idx * 10 + (init_name.at(i) - '0');
			++i;
		}

		// get the content of argv[i] from file "input_arguments.bin"
		// each content of argv[i] is devided by '\0'
		for (i = 0; i < argv_idx; ++i)
			getline(i_myfile, tc_token, '\0');

		// The format of tc_token is "name size value"
		stringstream ss_1(tc_token);
		getline(ss_1, input_name, ' '); // get "name" in the format of "argv[i]"
		getline(ss_1, input_size, ' '); // get "size"

		// As the value is a string and hence '\0' is used to be the delimiter.
		// getline(tc_ss, tc_token, '\0');
		getline(ss_1, input_value, '\0'); // get "value"

		// covert string "size" to integer "size"
		stringstream s2i_size(input_size);
		uint32_t int_size;
		s2i_size >> int_size;

#if defined(DBG_BO_CURRENT)
#if defined(CRETE_DEBUG)
		cerr << "argv_idx = " << argv_idx << '\n';

		cerr << "input_name = " << input_name << ", init_name = " << init_name << '\n'
		<< "input_size = " << input_size << ", init_size = " << init_size
		<< ", int_size = " << int_size << ", input_value = " << input_value << '\n';

		cerr << "The argv from test case that will be fed back is: \n"
		<< tc_token << '\n';
#endif // defined(CRETE_DEBUG)
#endif

		// Make sure the information match between test case in host and argv in guest
		assert(input_name.compare(init_name) == 0);
		assert(input_size.compare(init_size) == 0);
		assert(input_value.size() == int_size);

#ifdef DBG_BO_CURRENT
#if defined(CRETE_DEBUG)
		cerr << "getHostAddress is invoked in feed_test_case.\n";
    	cerr << "g_cpuState_bct: " << hex << (uint64_t)g_cpuState_bct
    			<< dec << endl;
#endif // defined(CRETE_DEBUG)
#endif

    	// Replace the value of args by values read from file
		tc_host_address = RuntimeEnv::getHostAddress(g_cpuState_bct,
				tc_guest_address, 1);

		if(tc_host_address != -1)
			memcpy((char *)tc_host_address, input_value.c_str(), input_value.size());

#if defined(CRETE_DEBUG)
		cerr << input_name << " address: " << tc_guest_address << ":" << tc_host_address << endl;
#endif // defined(CRETE_DEBUG)
	} // end of if(i_myfile)

	i_myfile.close();
}

static void guest_printf_handler(uint64_t msg_addr) {
	stringstream ss;
	string token;

	ss << (char *) msg_addr;
	getline(ss, token, '\n');

	if (token.compare(0, 14, "Dump MO start.") == 0) {
		dump_guest_memory(msg_addr);
	} else if (token.compare(0, 21, "Feed test case start.") == 0) {
		feed_test_case(msg_addr);
	}
}

int crete_is_include_filter_empty(void)
{
    if(g_pc_include_filters.empty())
        return 1;
    return 0;
}

//BOBO: touched for code refactoring of crete_runtime_dump
int crete_is_pc_in_exclude_filter_range(uint64_t pc)
{
    for(vector<PCFilter>::iterator iter = g_pc_exclude_filters.begin();
        iter != g_pc_exclude_filters.end();
        ++iter)
    {
        if(iter->is_in_range(pc))
            return 1;
    }
    return 0;
}

//BOBO: touched for code refactoring of crete_runtime_dump
int crete_is_pc_in_include_filter_range(uint64_t pc)
{
    for(vector<PCFilter>::iterator iter = g_pc_include_filters.begin();
        iter != g_pc_include_filters.end();
        ++iter)
    {
        if(iter->is_in_range(pc))
            return 1;
    }
    return 0;
}

int crete_is_pc_in_call_stack_exclude_filter_range(uint64_t pc)
{
  for(vector<PCFilter>::iterator iter = g_pc_call_stack_exclude_filters.begin();
      iter != g_pc_call_stack_exclude_filters.end();
      ++iter)
    {
        if(iter->is_in_range(pc))
            return 1;
    }
    return 0;
}

static void tcg_llvm_cleanup(void)
{
    if(tcg_llvm_ctx) {
        tcg_llvm_close(tcg_llvm_ctx);
        tcg_llvm_ctx = NULL;
    }
}
static void runtime_dump_cleanup(void)
{
    if(runtime_env) {
        runtime_dump_close(runtime_env);
        runtime_env = NULL;
    }
}

static void bct_tcg_custom_instruction_handler(uint64_t arg) {
#if defined(CRETE_DEBUG)
    if(arg != 0x02AE00)
        cout << "opcode = " << hex << arg << dec << endl;
#endif // defined(CRETE_DEBUG)

	switch (arg) {
    case CRETE_INSTR_MESSAGE_VALUE: 	// s2e_printf
	{
		target_ulong ptr_msg_virtual = 0;
		uint64_t ptr_msg_host = 0;

		RuntimeEnv::readCpuRegister(g_cpuState_bct, &ptr_msg_virtual,
				CPU_OFFSET(regs[R_EAX]), sizeof(ptr_msg_virtual));

#ifdef DBG_BO_CURRENT
#if defined(CRETE_DEBUG)
		cerr << "\ngetHostAddress is invoked in bct_tcg_custom_instruction_handler"
				" to get the address of pointer to the message from guest.\n";
    	cerr << "g_cpuState_bct: " << hex << (uint64_t)g_cpuState_bct
    			<< dec << endl;
#endif // defined(CRETE_DEBUG)
#endif
		ptr_msg_host = RuntimeEnv::getHostAddress(g_cpuState_bct,
				ptr_msg_virtual, 1);

#if defined(CRETE_DEBUG)
		cerr << "[Custom opc: 0x1000] Message from guest: "
				<< (char *) ptr_msg_host << endl;
#endif // defined(CRETE_DEBUG)

		try // Temporary "tee" bug workaround
		{
			guest_printf_handler(ptr_msg_host);
		}
		catch(std::runtime_error& e) // Temporary "tee" bug workaround
		{
#if defined(CRETE_DEBUG)
			cerr << "Exception: " << e.what() << endl;
#endif // defined(CRETE_DEBUG)
			g_custom_inst_emit = 0; // Temporarily disable dump.
			g_havlicek_target_pid = 0; // Temporarily disable dump.
			g_havlicek_tmp_workaround = true;
		}

//		guest_printf_handler(ptr_msg_host);

#ifdef DBG_BO_PAST
#if defined(CRETE_DEBUG)
		cerr << "bct_tcg_custom_instruction_handler: " << hex << arg << endl
		<< "g_cpuState_bct: " << (uint64) g_cpuState_bct << endl
		<< "ptr_msg_virtual: " << ptr_msg_virtual << endl
		<< "ptr_msg_host: " << ptr_msg_host << endl
		<< "msg: " << (char *)ptr_msg_host << endl;
#endif // defined(CRETE_DEBUG)
#endif

		break;
	}
    case CRETE_INSTR_QUIT_VALUE:
    {
        havlicek_qemu_system_shutdown_request();
        break;
    }
    case CRETE_INSTR_DUMP_VALUE:
    {
        if(flag_rt_dump_start == 0)
        {
#if defined(CRETE_DEBUG)
            printf("Error: attempting to dump: flag_rt_dump_start == 0! Dump flag was never set - aborting!\n");
#endif // defined(CRETE_DEBUG)
            return;
        }
        if(g_havlicek_tmp_workaround == true) // Temporary. May be false in case error (bug) happened and need to bypass dump.
        {
            g_custom_inst_emit = 0;
            g_havlicek_target_pid = 0;
//            g_pc_exclude_filters.clear();
//            g_pc_include_filters.clear();

            runtime_env->dumpConcolicData();
            if(crete_flag_write_initial_input)
            {
                runtime_env->dump_initial_input();

                crete_flag_write_initial_input = false;
            }

            // Release
            dump_printInfo(runtime_env);
            dump_writeRtEnvToFile(runtime_env, NULL);
            runtime_dump_cleanup(); // Cleanup must happen before tb_flush (or crash occurs).
            tb_flush(g_cpuState_bct); // Flush tb cache, so references to runtime_env/tcg_llvm_ctx are destroyed.
            // Reacquire
            runtime_env = runtime_dump_initialize();
            assert(runtime_env);

            // Release
            tcg_llvm_cleanup();
        }
        else // Normal execution.
        {
            g_custom_inst_emit = 0;
            g_havlicek_target_pid = 0;
//            g_pc_exclude_filters.clear();
//            g_pc_include_filters.clear();

            runtime_env->dumpConcolicData();
            if(crete_flag_write_initial_input)
            {
                runtime_env->dump_initial_input();

                crete_flag_write_initial_input = false;
            }

            // Release
            dump_printInfo(runtime_env);
            dump_writeRtEnvToFile(runtime_env, NULL);
            runtime_dump_cleanup(); // Cleanup must happen before tb_flush (or crash occurs).
            tb_flush(g_cpuState_bct); // Flush tb cache, so references to runtime_env/tcg_llvm_ctx are destroyed.
            // Reacquire
            runtime_env = runtime_dump_initialize();
            assert(runtime_env);

            // Release
            tcg_llvm_cleanup();
        }

        break;
    }
    case CRETE_INSTR_MAKE_CONCOLIC_VALUE: // Dump Memory Object (MO) value/addr start.
    {
        target_ulong guest_addr = g_cpuState_bct->regs[R_EAX];
        target_ulong size = g_cpuState_bct->regs[R_ECX];
        target_ulong name_guest_addr = g_cpuState_bct->regs[R_EDX];

#if defined(CRETE_DEBUG)
        cout << "CRETE_INSTR_MAKE_CONCOLIC_VALUE:\n"
             << "guest_addr: " << guest_addr << "\n"
             << "size: " << size << "\n"
             << "name_guest_addr: " << name_guest_addr << endl;
#endif // defined(CRETE_DEBUG)

        const char* name = (const char*)RuntimeEnv::getHostAddress(g_cpuState_bct, name_guest_addr, 1, 0);

        uint64_t host_addr = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr, 1);

        RuntimeEnv::ConcolicMemoryObject cmo(
                    string(name),
                    name_guest_addr,
                    guest_addr,
                    host_addr,
                    size
                    );

        runtime_env->addConcolicData(cmo);

        try // Temporary "tee" bug workaround
        {
#if !defined(TARGET_X86_64)
            //BOBO: xxx a quick hack here to filter the apparently incorrect guest address
            if(cmo.data_guest_addr_ <= 0x8048000) {
#if defined(CRETE_DEBUG)
                cerr << "[tee bug] happened and bypassed!\n";
#endif // defined(CRETE_DEBUG)
                stringstream ss;
                ss << "invalid guest address";
                ss << cmo.data_guest_addr_;
                throw runtime_error(ss.str());
            }
#endif // !defined(TARGET_X86_64)

            if(flag_is_first_iteration && !boost::filesystem::exists("hostfile/input_arguments.bin"))
            {
                memset((void*)cmo.data_host_addr_, 0, cmo.data_size_);

                crete_flag_write_initial_input = true;
            }
            else
            {
                runtime_env->feed_test_case("hostfile/input_arguments.bin");
            }
        }
        catch(std::runtime_error& e) // Temporary "tee" bug workaround
        {
#if defined(CRETE_DEBUG)
            cerr << "Exception: " << e.what() << endl;
#endif // defined(CRETE_DEBUG)
            g_custom_inst_emit = 0; // Temporarily disable dump.
            g_havlicek_target_pid = 0; // Temporarily disable dump.
            g_havlicek_tmp_workaround = true;
        }

        break;
    }
    case CRETE_INSTR_CAPTURE_BEGIN_VALUE: // Begin capture
    {
        g_havlicek_target_pid = g_cpuState_bct->cr[3];
        g_custom_inst_emit = 1;
        crete_flag_capture_enabled = 1;

#if defined(DBG_BO_CALL_STACK)
        // enable monitor call stack when a new iteration starts
		flag_enable_monitor_call_stack = 1;
		// reset call_stacK_started
		call_stack_started = false;
#endif

        reset_llvm();

        break;
    }
    case CRETE_INSTR_CAPTURE_END_VALUE: // End capture
    {
        g_havlicek_target_pid = 0;
        g_custom_inst_emit = 0;
        crete_flag_capture_enabled = 0;

#if defined(DBG_BO_CALL_STACK)
        flag_is_first_iteration = 0;
//        addr_main_function = 0;
//        size_main_function = 0;
//        g_crete_call_stack_bound = 0;

#if defined(DBG_BO_DYN_CALL_STACK) || 1
        runtime_env->finishCallStack();
#endif // #if defined(DBG_BO_DYN_CALL_STACK) || 1

#endif
        break;
    }
    case CRETE_INSTR_EXCLUDE_FILTER_VALUE: // Exclude filter
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];

#if defined(CRETE_DEBUG)
        cout << hex
             << "exclude filter addr range: "
             << addr_begin << "-" << addr_end
             << "; size: "
             << addr_end - addr_begin
             << dec
             << endl;
#endif // defined(CRETE_DEBUG)

        g_pc_exclude_filters.push_back(PCFilter(addr_begin, addr_end));

        break;
    }
    case CRETE_INSTR_INCLUDE_FILTER_VALUE: // Include filter
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];
#if defined(CRETE_DEBUG)
        cout << hex
             << "include filter addr range: "
             << addr_begin << "-" << addr_end
             << "; size: "
             << addr_end - addr_begin
             << dec
             << endl;
#endif // defined(CRETE_DEBUG)

        g_pc_include_filters.push_back(PCFilter(addr_begin, addr_end));

        break;
    }
    case CRETE_INSTR_PRIME_VALUE:
    {
        g_custom_inst_emit = 0;
        g_havlicek_target_pid = 0;

        g_pc_include_filters.clear();
        g_pc_exclude_filters.clear();
        elf_symtab_functions.clear();

#if defined(DBG_BO_CALL_STACK)
        //reset this flag when a new round of test begins
        flag_is_first_iteration = 1;
#endif

        break;
    }
#if defined(DBG_BO_CALL_STACK)
    case 0x07AE00: // Symtab function entries (one at a time)
    {
    	/* only collect the symtab function entries at the first iteration of crete workflow*/
    	if(flag_is_first_iteration) {
        	// Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
            target_ulong addr = g_cpuState_bct->regs[R_EAX];
            target_ulong size = g_cpuState_bct->regs[R_ECX];
            target_ulong func_name_addr = g_cpuState_bct->regs[R_EDX];

            const char* func_name = (const char*)RuntimeEnv::getHostAddress(g_cpuState_bct, func_name_addr, 1);

            // Collect symtab function entries in elf_symtab_functions
            CallStackEntry temp_cs_entry(addr, size, string(func_name));
            if(elf_symtab_functions.insert(pair<uint64_t, CallStackEntry>(addr, temp_cs_entry)).second) {
#if defined(CRETE_DEBUG) && 0
            	cout << "[Warning] elf_symtab_functions insert fail, because an existing key exsits.\n"
            			<< "\tto be added entry: addr = 0x" << hex << addr << ", size = 0x" << size
            			<< ", func_name = " << func_name << endl;
            	map<uint64_t, CallStackEntry>::iterator it = elf_symtab_functions.find(addr);
            	cout << "\t existed entry:addr = 0x" << hex << it->second.m_start_addr
            			<< ", size = 0x" << it->second.m_size
            			<< ", func_name = " << it->second.m_func_name
            			<< dec << endl << endl;
#endif
            }

            cout << hex;
            cout << "func_name: " << func_name << endl;
            cout << "addr: " << addr << endl;
            cout << "size: " << size << endl;
            cout << dec;
        }
        break;
    }

    case CRETE_INSTR_CALL_STACK_EXCLUDE_VALUE: // call-stack-exclude
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];
#if defined(CRETE_DEBUG) || 1
        cout << hex
             << "call stack execlude filter addr range: "
             << addr_begin << "-" << addr_end
             << "; size: "
             << addr_end - addr_begin
             << dec
             << endl;
#endif // defined(CRETE_DEBUG)

        g_pc_call_stack_exclude_filters.push_back(PCFilter(addr_begin, addr_end));

        break;
    }
    case CRETE_INSTR_CALL_STACK_SIZE_VALUE:
    {
        g_crete_call_stack_bound = g_cpuState_bct->regs[R_EAX];

#if defined(CRETE_DEBUG)
        cout << "call stack size: " << g_crete_call_stack_bound << "\n";
#endif // defined(CRETE_DEBUG)

        break;
    }
    case CRETE_INSTR_MAIN_ADDRESS_VALUE:
    {
    	addr_main_function = g_cpuState_bct->regs[R_EAX];
    	size_main_function = g_cpuState_bct->regs[R_ECX];

#if defined(CRETE_DEBUG)
        cout << "main_function: addr = 0x" << hex << addr_main_function
        		<< ", size = 0x" << size_main_function << dec << endl;
#endif // defined(CRETE_DEBUG)

        break;
    }
#endif//#if defined(DBG_BO_CALL_STACK)

		// Add new custom instruction handler here
		// Add new custom instruction handler here

	default:{
		assert(0 && "illegal operation: unsupported op code\n");
	}

	}

}

void bct_tcg_emit_custom_instruction(uint64_t arg) {
	TCGv_i64 t0 = tcg_temp_new_i64();
	tcg_gen_movi_i64(t0, arg);

	TCGArg args[1];
	args[0] = GET_TCGV_I64(t0);
    tcg_gen_helperN((void*) bct_tcg_custom_instruction_handler, 0, 2,
    TCG_CALL_DUMMY_ARG, 1, args);

	tcg_temp_free_i64(t0);
}


void crete_set_data_dir(const char* data_dir)
{
    namespace fs = boost::filesystem;

    fs::path dpath(data_dir);

    if(!fs::exists(dpath))
    {
        throw std::runtime_error("failed to find data directory: " + dpath.string());
    }

    crete_data_dir = dpath.parent_path().string();
}
