#ifndef TCG_LLVM_OFFLINE_H
#define TCG_LLVM_OFFLINE_H

#ifdef __cplusplus
extern "C" {
#endif

struct TCGLLVMOfflineContext;
extern const struct TCGLLVMOfflineContext *g_tcg_llvm_offline_ctx;

void x86_llvm_translator();

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

#include <vector>
#include <stdint.h>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

using namespace std;

class TLO_TCGContext;
struct TCGContext;
struct TCGTemp;
class TCGLLVMOfflineContext
{
private:
	// Required information from QEMU for the offline translation
	vector<uint64_t> m_tlo_tb_pc;

	vector<TCGContext> m_tcg_ctx;
	vector<vector<TCGTemp> > m_tcg_temps;
	map<uint64_t, string> m_helper_names;

	// QEMU IR opc and opparam for each TB that will be translated
	vector<vector<uint64_t> > m_gen_opc_buf;
	vector<vector<uint64_t> > m_gen_opparam_buf;

public:
	TCGLLVMOfflineContext() {};
	~TCGLLVMOfflineContext() {};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_tlo_tb_pc;

        ar & m_tcg_ctx;
        ar & m_tcg_temps;
        ar & m_helper_names;

        ar & m_gen_opc_buf;
        ar & m_gen_opparam_buf;
    }

    void dump_tlo_tb_pc(const uint64_t pc);

    void dump_tcg_ctx(const TCGContext& tcg_ctx);
    void dump_tcg_temp(const vector<TCGTemp>& tcg_temp);
    void dump_tcg_helper_name(const TCGContext &tcg_ctx);

    void dump_tlo_opc_buf(const uint64_t *opc_buf);
    void dump_tlo_opparam_buf(const uint64_t *opparam_buf);

    const uint64_t get_tlo_tb_pc(const uint64_t tb_index);

    const TCGContext& get_tcg_ctx(const uint64_t tb_index);
    const vector<TCGTemp>& get_tcg_temp(const uint64_t tb_index);
    const string get_helper_name(const uint64_t func_addr) const;

    const vector<uint64_t>& get_tlo_opc_buf(const uint64_t tb_index);
    const vector<uint64_t>& get_tlo_opparam_buf(const uint64_t tb_index);

    void print_info();
    void dump_verify();
    uint64_t get_size();
};

struct TCGTemp;
class TLO_TCGContext
{
public:
	int m_nb_labels;
    int m_nb_globals;

    vector<TCGTemp> m_temps;


    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
    	ar & m_nb_labels;
    	ar & m_nb_globals;

    	ar & m_temps;
    }
};

#endif // #ifdef __cplusplus

#endif //#ifndef TCG_LLVM_OFFLINE_H
