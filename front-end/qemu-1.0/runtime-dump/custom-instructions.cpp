#include "custom-instructions.h"
#include <boost/serialization/split_member.hpp>
#include <string>
#include <stdlib.h>
#include <boost/unordered_set.hpp>

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

#include "runtime-dump.h"
#include <tcg-llvm.h>
#include <crete/custom_opcode.h>
#include <crete/debug_flags.h>
#include <boost/system/system_error.hpp>
#include <boost/filesystem/fstream.hpp>

#if defined(CRETE_INPUT) || 1
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem/operations.hpp>
#endif // CRETE_INPUT || 1

#if defined(CRETE_PROFILE) || 1
#include <boost/date_time/posix_time/posix_time.hpp>
#include <valgrind/callgrind.h>
#endif // defined(CRETE_PROFILE) || 1

#include "runtime-dump/tci_analyzer.h"

using namespace std;
namespace fs = boost::filesystem;

extern RuntimeEnv *runtime_env;
#define CPU_OFFSET(field) offsetof(CPUState, field)

extern "C" {
extern CPUState *g_cpuState_bct;

//extern target_ulong g_crete_target_pid;
//extern int g_custom_inst_emit;
int crete_is_include_filter_empty(void);

int crete_is_pc_in_exclude_filter_range(uint64_t pc);
int crete_is_pc_in_include_filter_range(uint64_t pc);

#if defined(CRETE_DBG_CALL_STACK)
int crete_is_pc_in_call_stack_exclude_filter_range(uint64_t pc);
#endif

extern int flag_rt_dump_enable;

}

std::string crete_data_dir;

bool g_crete_tmp_workaround = false; // TODO temporary workaround for bug
static bool crete_flag_write_initial_input = false;

const std::string crete_trace_ready_file_name = "trace_ready";

class PCFilter
{
public:
    PCFilter(target_ulong addr_start, target_ulong addr_end) : addr_start_(addr_start), addr_end_(addr_end) {}
    bool is_in_range(target_ulong pc) { return pc >= addr_start_ && pc < addr_end_; } // Must be < addr_end_, not <= addr_end_.
private:
    target_ulong addr_start_, addr_end_;
};

static boost::unordered_set<uint64_t> g_pc_exclude_filters;
static boost::unordered_set<uint64_t> g_pc_include_filters;

#if defined(CRETE_DBG_CALL_STACK)
static boost::unordered_set<uint64_t> g_pc_call_stack_exclude_filters;
#endif

#if defined(CRETE_PROFILE) || 1
struct StopWatch
{
    boost::posix_time::ptime start_time;
    boost::posix_time::time_duration total_time;
} stop_watch;
std::size_t guest_replay_program_count = 0;
#endif // defined(CRETE_PROFILE) || 1

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
    string libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "op_helper.bc");
    tcg_linkWithLibrary(tcg_llvm_ctx, libraryName.c_str());

    libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete_helper.bc");
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

        // We don't know whether the "tee bug" exists in x64,
        // and which address range would indicate it, so we
        // only do the check on I386 for now.
#if !defined(TARGET_X86_64)
        if(guest_address <= 0x8048000) {
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
        if(tc_guest_address <= 0x8048000) {
            stringstream ss;
            ss << "invalid guest address";
            ss << tc_guest_address;
            throw runtime_error(ss.str());
        }
        assert(tc_guest_address > 0x8048000 && "invalid guest address in feed_test_case.\n");
#endif // !defined(TARGET_X86_64)

		getline(tc_ss, tc_token, '\0'); // Get init_value of argv[i]
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

		// Make sure the information match between test case in host and argv in guest
		assert(input_name.compare(init_name) == 0);
		assert(input_size.compare(init_size) == 0);
		assert(input_value.size() == int_size);

    	// Replace the value of args by values read from file
		tc_host_address = RuntimeEnv::getHostAddress(g_cpuState_bct,
				tc_guest_address, 1);

		if(tc_host_address != -1)
			memcpy((char *)tc_host_address, input_value.c_str(), input_value.size());
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

int crete_is_pc_in_exclude_filter_range(uint64_t pc)
{
    boost::unordered_set<uint64_t>::const_iterator it = g_pc_exclude_filters.find(pc);
    if(it != g_pc_exclude_filters.end()){
    	return 1;
    } else {
    	return 0;
    }
}

int crete_is_pc_in_include_filter_range(uint64_t pc)
{
    boost::unordered_set<uint64_t>::const_iterator it = g_pc_include_filters.find(pc);
    if(it != g_pc_include_filters.end()){
    	return 1;
    } else {
    	return 0;
    }
}

int crete_is_pc_in_call_stack_exclude_filter_range(uint64_t pc)
{
    boost::unordered_set<uint64_t>::const_iterator it = g_pc_call_stack_exclude_filters.find(pc);
    if(it != g_pc_call_stack_exclude_filters.end()){
    	return 1;
    } else {
    	return 0;
    }
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

struct PIDWriter
{
    PIDWriter()
    {
        fs::path dir = "hostfile";

        if(!fs::exists(dir))
        {
        fs::create_directories(dir);
        }

        fs::path p = dir / "pid";

        if(fs::exists(p))
        {
            fs::remove(p);
        }

        fs::ofstream ofs(p,
                 ios_base::binary | ios_base::out);

        if(!ofs.good())
        {
            assert(0 && "Could not open hostfile/pid for writing");
        }

        ofs << ::getpid();
    }
} pid_writer; // Ctor writes PID when program starts.

static void verify_contiguous_host_address(uint64_t guest_addr, uint64_t size)
{
    uint64_t host_addr = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr, 1);
    for(uint64_t i = 1; i < size; ++i)
    {
        uint64_t host_addr_next = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr + i, 1);
        if(host_addr_next != host_addr + 1)
        {
            fs::ofstream ofs("error.log");
            ofs << "host_addr_next: " << host_addr_next << " != " << "host_addr + 1: " << host_addr + 1 << endl;
        }
        assert(host_addr_next == host_addr + 1);
        host_addr = host_addr_next;
    }

}

static void bct_tcg_custom_instruction_handler(uint64_t arg) {
	switch (arg) {
    case CRETE_INSTR_MESSAGE_VALUE: 	// s2e_printf
	{
		target_ulong ptr_msg_virtual = 0;
		uint64_t ptr_msg_host = 0;

		RuntimeEnv::readCpuRegister(g_cpuState_bct, &ptr_msg_virtual,
				CPU_OFFSET(regs[R_EAX]), sizeof(ptr_msg_virtual));

		ptr_msg_host = RuntimeEnv::getHostAddress(g_cpuState_bct,
				ptr_msg_virtual, 1);

		try // Temporary "tee" bug workaround
		{
			guest_printf_handler(ptr_msg_host);
		}
		catch(std::runtime_error& e) // Temporary "tee" bug workaround
		{
			g_custom_inst_emit = 0; // Temporarily disable dump.
			g_crete_target_pid = 0; // Temporarily disable dump.
			g_crete_tmp_workaround = true;
		}

//		guest_printf_handler(ptr_msg_host);

		break;
	}
    case CRETE_INSTR_QUIT_VALUE:
    {
        crete_qemu_system_shutdown_request();
        break;
    }
    case CRETE_INSTR_DUMP_VALUE:
    {
        namespace fs = boost::filesystem;

        if(flag_rt_dump_start == 0)
        {
            return;
        }

        while(fs::exists(crete_trace_ready_file_name))
            ; // Wait for it to not exist. TODO: not efficient.

        if(g_crete_tmp_workaround == true) // Temporary. May be false in case error (bug) happened and need to bypass dump.
        {
            g_custom_inst_emit = 0;
            g_crete_target_pid = 0;
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
            g_crete_target_pid = 0;
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

            crete_tci_next_iteration();
        }

        {
            fs::ofstream ofs(fs::path("hostfile") / crete_trace_ready_file_name);

            if(!ofs.good())
            {
                assert(0 && "can't write to crete_trace_ready_file_name");
            }
        }

        break;
    }
    case CRETE_INSTR_MAKE_CONCOLIC_VALUE: // Dump Memory Object (MO) value/addr start.
    {
        target_ulong guest_addr = g_cpuState_bct->regs[R_EAX];
        target_ulong size = g_cpuState_bct->regs[R_ECX];
        target_ulong name_guest_addr = g_cpuState_bct->regs[R_EDX];

        const char* name = (const char*)RuntimeEnv::getHostAddress(g_cpuState_bct, name_guest_addr, 1, 0);

        // verify_contiguous_host_address(name_guest_addr, size);

        uint64_t host_addr = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr, 1);

        RuntimeEnv::ConcolicMemoryObject cmo(
                    string(name),
                    name_guest_addr,
                    guest_addr,
                    host_addr,
                    size
                    );

        runtime_env->addConcolicData(cmo);

        crete_tci_make_symbolic(guest_addr, size);
        crete_tci_next_block();

        try // Temporary "tee" bug workaround
        {
#if !defined(TARGET_X86_64)
            if(cmo.data_guest_addr_ <= 0x8048000) {
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
            g_custom_inst_emit = 0; // Temporarily disable dump.
            g_crete_target_pid = 0; // Temporarily disable dump.
            g_crete_tmp_workaround = true;
        }

        break;
    }
    case CRETE_INSTR_CAPTURE_BEGIN_VALUE: // Begin capture
    {
#if defined(CRETE_PROFILE)
       CALLGRIND_START_INSTRUMENTATION;
#endif // defined(CRETE_PROFILE)

        g_crete_target_pid = g_cpuState_bct->cr[3];
        g_custom_inst_emit = 1;
        crete_flag_capture_enabled = 1;

#if defined(CRETE_DBG_CALL_STACK)
        // enable monitor call stack when a new iteration starts
		flag_enable_monitor_call_stack = 1;
        // reset call_stack_started
		call_stack_started = false;
#endif

        reset_llvm();

        break;
    }
    case CRETE_INSTR_CAPTURE_END_VALUE: // End capture
    {
#if defined(CRETE_PROFILE)
       CALLGRIND_STOP_INSTRUMENTATION;
#endif // defined(CRETE_PROFILE)

        g_crete_target_pid = 0;
        g_custom_inst_emit = 0;
        crete_flag_capture_enabled = 0;

#if defined(CRETE_DBG_CALL_STACK)
        flag_is_first_iteration = 0;

        runtime_env->finishCallStack();
#endif //#if defined(CRETE_DBG_CALL_STACK)

        break;
    }
    case CRETE_INSTR_EXCLUDE_FILTER_VALUE: // Exclude filter
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];

        for(uint64_t i = addr_begin; i < addr_end; ++i) {
        	g_pc_exclude_filters.insert(i);
        }

        break;
    }
    case CRETE_INSTR_INCLUDE_FILTER_VALUE: // Include filter
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];
        for(uint64_t i = addr_begin; i < addr_end; ++i) {
        	g_pc_include_filters.insert(i);
        }

        break;
    }
    case CRETE_INSTR_PRIME_VALUE:
    {
        g_custom_inst_emit = 0;
        g_crete_target_pid = 0;

        g_pc_include_filters.clear();
        g_pc_exclude_filters.clear();
        elf_symtab_functions.clear();
        g_pc_call_stack_exclude_filters.clear();

#if defined(CRETE_DBG_CALL_STACK)
        //reset this flag when a new round of test begins
        flag_is_first_iteration = 1;
#endif

        break;
    }
#if defined(CRETE_DBG_CALL_STACK)
    case 0x07AE00: // Symtab function entries (one at a time)
    {
    	/* only collect the symtab function entries at the first iteration of crete workflow*/
    	if(flag_is_first_iteration) {
        	// Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
            target_ulong addr = g_cpuState_bct->regs[R_EAX];
            target_ulong size = g_cpuState_bct->regs[R_ECX];
//            target_ulong func_name_addr = g_cpuState_bct->regs[R_EDX];

//            cout << "0x07AE00 begins." << endl;
//            const char* func_name = (const char*)RuntimeEnv::getHostAddress(g_cpuState_bct, func_name_addr, );
            const char* func_name = "symtab_funcs";
//            cout << "0x07AE00 finishes." << endl;

            // Collect symtab function entries in elf_symtab_functions
            CallStackEntry temp_cs_entry(addr, size, string(func_name));
            if(elf_symtab_functions.insert(pair<uint64_t, CallStackEntry>(addr, temp_cs_entry)).second) {
            }
        }
        break;
    }

    case CRETE_INSTR_CALL_STACK_EXCLUDE_VALUE: // call-stack-exclude
    {
        // Note: using ecx/eax is safe for 64bit, as regs[] is defined as target_ulong, and there's no R_RCX/R_RAX
        target_ulong addr_begin = g_cpuState_bct->regs[R_EAX];
        target_ulong addr_end = g_cpuState_bct->regs[R_ECX];
        for(uint64_t i = addr_begin; i < addr_end; ++i) {
        	g_pc_call_stack_exclude_filters.insert(i);
        	g_pc_exclude_filters.insert(i);
        }

        break;
    }
    case CRETE_INSTR_CALL_STACK_SIZE_VALUE:
    {
      g_crete_call_stack_bound = g_cpuState_bct->regs[R_EAX];
        break;
    }
    case CRETE_INSTR_MAIN_ADDRESS_VALUE:
    {
    	addr_main_function = g_cpuState_bct->regs[R_EAX];
    	size_main_function = g_cpuState_bct->regs[R_ECX];

        break;
    }
#endif//#if defined(CRETE_DBG_CALL_STACK)

    // To be removed
    case CRETE_INSTR_LIBC_START_MAIN_ADDRESS_VALUE:
    {
        break;
    }
    case CRETE_INSTR_LIBC_EXIT_ADDRESS_VALUE:
    {
        break;
    }
    case CRETE_INSTR_STACK_DEPTH_BOUNDS_VALUE:
    {
        break;
    }
#if defined(CRETE_PROFILE) || 1
    case CRETE_INSTR_START_STOPWATCH_VALUE:
    {
        stop_watch.start_time = boost::posix_time::microsec_clock::local_time();
        ++guest_replay_program_count;

        break;
    }
    case CRETE_INSTR_STOP_STOPWATCH_VALUE:
    {
        assert(stop_watch.start_time.is_not_a_date_time() == false);

        boost::posix_time::time_duration diff =
            boost::posix_time::microsec_clock::local_time() - stop_watch.start_time;

        stop_watch.total_time += diff;

        break;
    }
    case CRETE_INSTR_RESET_STOPWATCH_VALUE:
    {
        std::cerr << "stopwatch: " << stop_watch.total_time.total_microseconds()
                  << ", " << guest_replay_program_count
                  << endl;

        stop_watch.total_time = boost::posix_time::time_duration();
        guest_replay_program_count = 0;

        break;
    }
    case CRETE_INSTR_REPLAY_NEXT_PROGRAM_VALUE:
    {
//        std::cout << "CRETE_INSTR_REPLAY_NEXT_PROGRAM_VALUE" << std::endl;
        namespace fs = boost::filesystem;

        fs::path next_file = "next-exec.txt";

        assert(fs::exists(next_file));

        fs::ifstream ifs(next_file);

        assert(ifs.good());

        target_ulong guest_addr = g_cpuState_bct->regs[R_EAX];
        target_ulong size = g_cpuState_bct->regs[R_ECX];

        uint64_t host_addr = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr, 1);

        std::string prog_name;

        ifs >> prog_name;

        assert(prog_name.size() < size);

        memcpy((void*)host_addr, (void*)prog_name.c_str(), prog_name.size() + 1);

        break;
    }
#endif // define(CRETE_PROFILE) || 1
    case CRETE_INSTR_READ_PORT_VALUE:
    {
        target_ulong guest_addr = g_cpuState_bct->regs[R_EAX];
        target_ulong size = g_cpuState_bct->regs[R_ECX];

        uint64_t host_addr = RuntimeEnv::getHostAddress(g_cpuState_bct, guest_addr, 1);

        unsigned short port = 0;

        const char* file_path = "hostfile/port";

        if(fs::exists(file_path))
        {
            fs::ifstream ifs(file_path,
                             std::ios_base::in | std::ios_base::binary);

            assert(ifs.good());

            ifs >> port;
        }

        assert(size == sizeof(port));

        memcpy((void*)host_addr,
               (void*)&port,
               size);

        break;
    }

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
