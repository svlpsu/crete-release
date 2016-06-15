#include "tcg-llvm-offline.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <string>
#include <stdlib.h>

extern "C" {
#include "tcg.h"
}

#include "tcg-llvm.h"
#include "runtime-dump/custom-instructions.h"

#include <iostream>
#include <fstream>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

using namespace std;

extern "C" {
const TCGLLVMOfflineContext *g_tcg_llvm_offline_ctx = 0;
}

void TCGLLVMOfflineContext::dump_tlo_tb_pc(const uint64_t pc)
{
	m_tlo_tb_pc.push_back(pc);
}

void TCGLLVMOfflineContext::dump_tcg_ctx(const TCGContext& tcg_ctx)
{
	m_tcg_ctx.push_back(tcg_ctx);
}

void TCGLLVMOfflineContext::dump_tcg_temp(const vector<TCGTemp>& tcg_temp)
{
	m_tcg_temps.push_back(tcg_temp);
}

void TCGLLVMOfflineContext::dump_tcg_helper_name(const TCGContext &tcg_ctx)
{
    const TCGHelperInfo *ptr_helpers = tcg_ctx.helpers;
    uint64_t helper_addr;
    string helper_name;
	for(uint64_t i = 0; i < tcg_ctx.nb_helpers; ++i){
		helper_addr = (uint64_t)ptr_helpers[i].func;
		helper_name = ptr_helpers[i].name;
		m_helper_names.insert(make_pair(helper_addr, helper_name));
	}

	cout << "dump_tcg_helper_name is invoked: m_helper_names.size() = "
			<< m_helper_names.size() << endl;
}

void TCGLLVMOfflineContext::dump_tlo_opc_buf(const uint64_t *opc_buf)
{
	vector<uint64> temp_opc_buf;
	temp_opc_buf.reserve(OPC_BUF_SIZE);

	for(uint32_t i = 0; i < OPC_BUF_SIZE; ++i)
		temp_opc_buf.push_back(opc_buf[i]);
	assert(temp_opc_buf.size() == OPC_BUF_SIZE);

	m_gen_opc_buf.push_back(temp_opc_buf);
}

void TCGLLVMOfflineContext::dump_tlo_opparam_buf(const uint64_t *opparam_buf)
{
	vector<uint64> temp_opparam_buf;
	temp_opparam_buf.reserve(OPPARAM_BUF_SIZE);

	for(uint32_t i = 0; i < OPPARAM_BUF_SIZE; ++i)
		temp_opparam_buf.push_back(opparam_buf[i]);
	assert(temp_opparam_buf.size() == OPPARAM_BUF_SIZE);

	m_gen_opparam_buf.push_back(temp_opparam_buf);;
}

const uint64_t TCGLLVMOfflineContext::get_tlo_tb_pc(const uint64_t tb_index)
{
	return m_tlo_tb_pc[tb_index];
}

const TCGContext& TCGLLVMOfflineContext::get_tcg_ctx(const uint64_t tb_index)
{
    return m_tcg_ctx[tb_index];
}

const vector<TCGTemp>& TCGLLVMOfflineContext::get_tcg_temp(const uint64_t tb_index)
{
	return m_tcg_temps[tb_index];
}

const string TCGLLVMOfflineContext::get_helper_name(const uint64_t func_addr) const
{
	map<uint64_t, string>::const_iterator it = m_helper_names.find(func_addr);
	assert(it != m_helper_names.end() &&
			"[CRETE ERROR] Can not find the helper function name based the given func_addr.\n");

	return it->second;
}

const vector<uint64_t>& TCGLLVMOfflineContext::get_tlo_opc_buf(const uint64_t tb_index)
{
	return m_gen_opc_buf[tb_index];
}

const vector<uint64_t>& TCGLLVMOfflineContext::get_tlo_opparam_buf(const uint64_t tb_index)
{
	return m_gen_opparam_buf[tb_index];
}

void TCGLLVMOfflineContext::print_info()
{
	cout  << dec << "m_tlo_tb_pc.size() = " << m_tlo_tb_pc.size() << endl
			<< "m_helper_names.size() = " << m_helper_names.size() << endl
			<< ", m_gen_opc_buf.size() = " << m_gen_opc_buf.size() << endl
			<< ", m_gen_opparam_buf.size() = " << m_gen_opparam_buf.size()
			<< endl;

	cout  << dec << "sizeof (m_tlo_tb_pc) = " << sizeof(m_tlo_tb_pc)<< endl
			<< "sizeof(m_helper_names) = " <<  sizeof(m_tcg_ctx)
			<< "sizeof(m_tcg_temps) = " << sizeof(m_tcg_temps)
			<< "sizeof(m_helper_names) = " << sizeof(m_helper_names) << endl
			<< ", sizeof(m_gen_opc_buf) = " << sizeof(m_gen_opc_buf)<< endl
			<< ", sizeof(m_gen_opparam_buf) = " << sizeof(m_gen_opparam_buf)
			<< endl;


	cout << "pc values: ";
	uint64_t j = 0;
	for(vector<uint64_t>::iterator it = m_tlo_tb_pc.begin();
			it != m_tlo_tb_pc.end(); ++it) {
		cout << "tb-" << dec << j++ << ": pc = 0x" << hex << (*it) << endl;
	}


}
void TCGLLVMOfflineContext::dump_verify()
{
	assert(m_tlo_tb_pc.size() == m_tcg_ctx.size());
	assert(m_tlo_tb_pc.size() == m_tcg_temps.size());
	assert(m_tlo_tb_pc.size() == m_gen_opc_buf.size());
	assert(m_tlo_tb_pc.size() == m_gen_opparam_buf.size());
}
uint64_t TCGLLVMOfflineContext::get_size()
{
	return (uint64_t)m_tlo_tb_pc.size();
}

/*****************************/
/* Functions for QEMU c code */
void x86_llvm_translator()
{
	cout << "this is the new main function from tcg-llvm-offline.\n" << endl;

	TranslationBlock temp_tb = {};
	TCGContext *s = &tcg_ctx;

	//1. initialize llvm dependencies
    tcg_llvm_ctx = tcg_llvm_initialize();
    assert(tcg_llvm_ctx);

    string libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-1.0-op-helper-i386.bc");
    tcg_linkWithLibrary(tcg_llvm_ctx, libraryName.c_str());

    libraryName =  crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-1.0-crete-helper-i386.bc");
    tcg_linkWithLibrary(tcg_llvm_ctx, libraryName.c_str());

    tcg_llvm_initHelper(tcg_llvm_ctx);

    //2. initialize tcg_llvm_ctx_offline
//    tcg_llvm_offline_ctx = new TCGLLVMOfflineContext();

    TCGLLVMOfflineContext temp_tcg_llvm_offline_ctx;

    ifstream ifs("dump_tcg_llvm_offline.bin");
    boost::archive::binary_iarchive ia(ifs);
    ia >> temp_tcg_llvm_offline_ctx;

#if defined(CRETE_DEBUG)
    temp_tcg_llvm_offline_ctx.print_info();
#endif

    temp_tcg_llvm_offline_ctx.dump_verify();

    g_tcg_llvm_offline_ctx = &temp_tcg_llvm_offline_ctx;

    //3. Translate
	TCGTemp temp_tcg_temps[TCG_MAX_TEMPS];
    for(uint64_t i = 0; i < temp_tcg_llvm_offline_ctx.get_size(); ++i) {
        //3.1 update temp_tb
    	temp_tb.pc = (target_long)temp_tcg_llvm_offline_ctx.get_tlo_tb_pc(i);

        //3.2 update tcg_ctx
    	const TCGContext temp_tcg_ctx = temp_tcg_llvm_offline_ctx.get_tcg_ctx(i);
    	memcpy((void *)s, (void *)&temp_tcg_ctx, sizeof(TCGContext));

    	assert(s->temps == NULL);
    	s->temps = temp_tcg_temps;

    	//3.3 update tcg_ctx->temps
    	const vector<TCGTemp> temp_tcg_temp = temp_tcg_llvm_offline_ctx.get_tcg_temp(i);
    	assert( temp_tcg_temp.size() == s->nb_temps);
    	for(uint64_t j = 0; j < s->nb_temps; ++j)
    		temp_tcg_temps[j].assign(temp_tcg_temp[j]);

    	//3.4 update gen_opc_buf and gen_opparam_buf
    	const vector<uint64_t>& temp_opc_buf = temp_tcg_llvm_offline_ctx.get_tlo_opc_buf(i);
    	assert(temp_opc_buf.size() == OPC_BUF_SIZE);
    	for(uint64_t j = 0; j < OPC_BUF_SIZE; ++j)
    		gen_opc_buf[j] = (uint16_t)temp_opc_buf[j];

    	const vector<uint64_t>& temp_opparam_buf = temp_tcg_llvm_offline_ctx.get_tlo_opparam_buf(i);
    	assert(temp_opparam_buf.size() == OPPARAM_BUF_SIZE);
    	for(uint64_t j = 0; j < OPPARAM_BUF_SIZE; ++j)
    		gen_opparam_buf[j] = (TCGArg)temp_opparam_buf[j];

        //3.5 generate llvm bitcode
    	cout << "tcg_llvm_ctx->generateCode(s, &temp_tb) will be invoked." << endl;
        temp_tb.tcg_llvm_context = NULL;
        temp_tb.llvm_function = NULL;

    	tcg_llvm_ctx->generateCode(s, &temp_tb);
    	cout << "tcg_llvm_ctx->generateCode(s, &temp_tb) is done." << endl;


    	assert(temp_tb.tcg_llvm_context != NULL);
        assert(temp_tb.llvm_function != NULL);

//        break;
    }


	//4. Write out the translated llvm bitcode to file in the current folder
	llvm::sys::Path bitcode_path = llvm::sys::Path::GetCurrentDirectory();
	bitcode_path.appendComponent("dump_llvm_offline.bc");
    tcg_llvm_ctx->writeBitCodeToFile(bitcode_path.str());

    //5. cleanup
//    delete tcg_llvm_offline_ctx;
}
