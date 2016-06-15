#include "tci_analyzer.h"

#include <boost/array.hpp>
#include <boost/unordered_set.hpp>

#include <iostream> // testing
#include <fstream>

namespace crete
{
namespace tci
{

const size_t register_count = 16u;

struct Reg
{
    Reg() : symbolic(false) {}
    bool symbolic;
};

struct Block
{
    Block()
        : symbolic_block_(false) {}

    bool symbolic_block_;
};

struct InstrRange
{
    InstrRange(uint32_t begin,
               uint32_t end)
        : begin_(begin)
        , end_(end) {}

    uint32_t begin_;
    uint32_t end_;
};

class Analyzer
{
public:
    typedef boost::unordered_set<uint64_t> MemSet;
public:
    void terminate_block();

    bool is_reg_symbolic(uint64_t index);
    void make_reg_symbolic(uint64_t index);
    void make_reg_concrete(uint64_t index);

    bool is_guest_mem_symbolic(uint64_t addr);
    void make_guest_mem_symbolic(uint64_t addr);
    void make_guest_mem_concrete(uint64_t addr);

    bool is_host_mem_symbolic(uint64_t addr);
    void make_host_mem_symbolic(uint64_t addr);
    void make_host_mem_concrete(uint64_t addr);

    bool is_block_symbolic();
    void mark_block_symbolic();

private:
    Block current_block_;
    boost::array<Reg, register_count> reg_;
    MemSet guest_mem_;
    MemSet host_mem_;
};

void Analyzer::terminate_block()
{
    current_block_ = Block();
}

bool Analyzer::is_reg_symbolic(uint64_t index)
{
    return reg_[index].symbolic;
}

void Analyzer::make_reg_symbolic(uint64_t index)
{
    assert(index < reg_.size());

    reg_[index].symbolic = true;

    mark_block_symbolic();

    std::cerr << "make_reg_symbolic[" << std::dec << index <<"]\n";
}

void Analyzer::make_reg_concrete(uint64_t index)
{
    assert(index < reg_.size());

    reg_[index].symbolic = false;

    if(reg_[index].symbolic)
        std::cerr << "make_reg_concrete[" << std::dec << index <<"]\n";
}

bool Analyzer::is_guest_mem_symbolic(uint64_t addr)
{
    MemSet::const_iterator it = guest_mem_.find(addr);

    return it != guest_mem_.end();
}

void Analyzer::make_guest_mem_symbolic(uint64_t addr)
{
    MemSet::iterator it = guest_mem_.find(addr);

    if(it == guest_mem_.end())
    {
        guest_mem_.insert(addr);
    }

    mark_block_symbolic();
}

void Analyzer::make_guest_mem_concrete(uint64_t addr)
{
    MemSet::const_iterator it = guest_mem_.find(addr);

    if(it != guest_mem_.end())
    {
        guest_mem_.erase(it);
    }
}

bool Analyzer::is_host_mem_symbolic(uint64_t addr)
{
    MemSet::const_iterator it = host_mem_.find(addr);

    return it != host_mem_.end();
}

void Analyzer::make_host_mem_symbolic(uint64_t addr)
{
    MemSet::iterator it = host_mem_.find(addr);

    if(it == host_mem_.end())
    {
        host_mem_.insert(addr);
    }

    mark_block_symbolic();
}

void Analyzer::make_host_mem_concrete(uint64_t addr)
{
    MemSet::const_iterator it = host_mem_.find(addr);

    if(it != host_mem_.end())
    {
        host_mem_.erase(it);
    }
}

bool Analyzer::is_block_symbolic()
{
    return current_block_.symbolic_block_;
}

void Analyzer::mark_block_symbolic()
{
    current_block_.symbolic_block_ = true;
}

static Analyzer analyzer;
static bool is_block_branching = false;

inline
bool is_current_block_symbolic()
{
    return analyzer.is_block_symbolic();
}

inline
void mark_block_symbolic()
{
    analyzer.mark_block_symbolic();
}

inline
bool is_reg_symbolic(uint64_t t)
{
    return analyzer.is_reg_symbolic(t);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1)
{
    return is_reg_symbolic(t0)
        || is_reg_symbolic(t1);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2)
{
    return is_reg_symbolic(t0, t1)
        || is_reg_symbolic(t2);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3)
{
    return is_reg_symbolic(t0, t1, t2)
        || is_reg_symbolic(t3);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4)
{
    return is_reg_symbolic(t0, t1, t2, t3)
        || is_reg_symbolic(t4);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4, uint64_t t5)
{
    return is_reg_symbolic(t0, t1, t2, t3, t4)
        || is_reg_symbolic(t5);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4, uint64_t t5, uint64_t t6)
{
    return is_reg_symbolic(t0, t1, t2, t3, t4, t5)
        || is_reg_symbolic(t6);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4, uint64_t t5, uint64_t t6, uint64_t t7)
{
    return is_reg_symbolic(t0, t1, t2, t3, t4, t5, t6)
        || is_reg_symbolic(t7);
}

inline
bool is_reg_symbolic(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4, uint64_t t5, uint64_t t6, uint64_t t7, uint64_t t8)
{
    return is_reg_symbolic(t0, t1, t2, t3, t4, t5, t6, t7)
        || is_reg_symbolic(t8);
}

inline
void make_reg_symbolic(uint64_t index)
{
    analyzer.make_reg_symbolic(index);
}

inline
void make_reg_concrete(uint64_t index)
{
    analyzer.make_reg_concrete(index);
}

inline
void terminate_block()
{
    analyzer.terminate_block();
    is_block_branching = false;
}

inline
bool is_guest_mem_symbolic(uint64_t addr)
{
    return analyzer.is_guest_mem_symbolic(addr);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2)
{
    return is_guest_mem_symbolic(addr1)
        || is_guest_mem_symbolic(addr2);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    return is_guest_mem_symbolic(addr1, addr2)
        || is_guest_mem_symbolic(addr3);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    return is_guest_mem_symbolic(addr1, addr2, addr3)
        || is_guest_mem_symbolic(addr4);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                           uint64_t addr5)
{
    return is_guest_mem_symbolic(addr1, addr2, addr3, addr4)
        || is_guest_mem_symbolic(addr5);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                           uint64_t addr5, uint64_t addr6)
{
    return is_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5)
        || is_guest_mem_symbolic(addr6);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                           uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    return is_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6)
        || is_guest_mem_symbolic(addr7);
}

inline
bool is_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                           uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    return is_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6, addr7)
        || is_guest_mem_symbolic(addr8);
}

inline
void make_guest_mem_symbolic(uint64_t addr)
{
    analyzer.make_guest_mem_symbolic(addr);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2)
{
    make_guest_mem_symbolic(addr1);
    make_guest_mem_symbolic(addr2);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    make_guest_mem_symbolic(addr1, addr2);
    make_guest_mem_symbolic(addr3);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    make_guest_mem_symbolic(addr1, addr2, addr3);
    make_guest_mem_symbolic(addr4);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5)
{
    make_guest_mem_symbolic(addr1, addr2, addr3, addr4);
    make_guest_mem_symbolic(addr5);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6)
{
    make_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5);
    make_guest_mem_symbolic(addr6);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    make_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6);
    make_guest_mem_symbolic(addr7);
}

inline
void make_guest_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    make_guest_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6, addr7);
    make_guest_mem_symbolic(addr8);
}

inline
void make_guest_mem_concrete(uint64_t addr)
{
    analyzer.make_guest_mem_concrete(addr);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2)
{
    make_guest_mem_concrete(addr1);
    make_guest_mem_concrete(addr2);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    make_guest_mem_concrete(addr1, addr2);
    make_guest_mem_concrete(addr3);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    make_guest_mem_concrete(addr1, addr2, addr3);
    make_guest_mem_concrete(addr4);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5)
{
    make_guest_mem_concrete(addr1, addr2, addr3, addr4);
    make_guest_mem_concrete(addr5);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6)
{
    make_guest_mem_concrete(addr1, addr2, addr3, addr4, addr5);
    make_guest_mem_concrete(addr6);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    make_guest_mem_concrete(addr1, addr2, addr3, addr4, addr5, addr6);
    make_guest_mem_concrete(addr7);
}

inline
void make_guest_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                             uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    make_guest_mem_concrete(addr1, addr2, addr3, addr4, addr5, addr6, addr7);
    make_guest_mem_concrete(addr8);
}

inline
bool is_host_mem_symbolic(uint64_t addr)
{
    return analyzer.is_host_mem_symbolic(addr);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2)
{
    return is_host_mem_symbolic(addr1)
        || is_host_mem_symbolic(addr2);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    return is_host_mem_symbolic(addr1, addr2)
        || is_host_mem_symbolic(addr3);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    return is_host_mem_symbolic(addr1, addr2, addr3)
        || is_host_mem_symbolic(addr4);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                          uint64_t addr5)
{
    return is_host_mem_symbolic(addr1, addr2, addr3, addr4)
        || is_host_mem_symbolic(addr5);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                          uint64_t addr5, uint64_t addr6)
{
    return is_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5)
        || is_host_mem_symbolic(addr6);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                          uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    return is_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6)
        || is_host_mem_symbolic(addr7);
}

inline
bool is_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                          uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    return is_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6, addr7)
        || is_host_mem_symbolic(addr8);
}

inline
void make_host_mem_symbolic(uint64_t addr)
{
    analyzer.make_host_mem_symbolic(addr);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2)
{
    make_host_mem_symbolic(addr1);
    make_host_mem_symbolic(addr2);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    make_host_mem_symbolic(addr1, addr2);
    make_host_mem_symbolic(addr3);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    make_host_mem_symbolic(addr1, addr2, addr3);
    make_host_mem_symbolic(addr4);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5)
{
    make_host_mem_symbolic(addr1, addr2, addr3, addr4);
    make_host_mem_symbolic(addr5);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6)
{
    make_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5);
    make_host_mem_symbolic(addr6);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    make_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6);
    make_host_mem_symbolic(addr7);
}

inline
void make_host_mem_symbolic(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    make_host_mem_symbolic(addr1, addr2, addr3, addr4, addr5, addr6, addr7);
    make_host_mem_symbolic(addr8);
}

inline
void make_host_mem_concrete(uint64_t addr)
{
    analyzer.make_host_mem_concrete(addr);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2)
{
    make_host_mem_concrete(addr1);
    make_host_mem_concrete(addr2);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3)
{
    make_host_mem_concrete(addr1, addr2);
    make_host_mem_concrete(addr3);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4)
{
    make_host_mem_concrete(addr1, addr2, addr3);
    make_host_mem_concrete(addr4);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5)
{
    make_host_mem_concrete(addr1, addr2, addr3, addr4);
    make_host_mem_concrete(addr5);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6)
{
    make_host_mem_concrete(addr1, addr2, addr3, addr4, addr5);
    make_host_mem_concrete(addr6);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6, uint64_t addr7)
{
    make_host_mem_concrete(addr1, addr2, addr3, addr4, addr5, addr6);
    make_host_mem_concrete(addr7);
}

inline
void make_host_mem_concrete(uint64_t addr1, uint64_t addr2, uint64_t addr3, uint64_t addr4,
                            uint64_t addr5, uint64_t addr6, uint64_t addr7, uint64_t addr8)
{
    make_host_mem_concrete(addr1, addr2, addr3, addr4, addr5, addr6, addr7);
    make_host_mem_concrete(addr8);
}

inline
void mark_block_branching()
{
    is_block_branching = true;
}

} // namespace tci
} // namespace crete


using namespace crete::tci;

static
bool crete_read_was_symbolic = false;

bool crete_is_current_block_symbolic()
{
    return is_current_block_symbolic();
}

// TODO: should be renamed to crete_tci_reg_monitor_reset - more descriptive?
void crete_tci_reg_monitor_begin(void)
{
    crete_read_was_symbolic = false;
}

void crete_tci_read_reg(uint64_t index)
{
    if(is_reg_symbolic(index))
    {
        crete_read_was_symbolic = true;

        // Need to mark the entire block as symbolic to be on the safe side.
        // Symbolic addresses especially can be missed otherwise.
        mark_block_symbolic();
    }
}

void crete_tci_write_reg(uint64_t index)
{
    if(crete_read_was_symbolic)
    {
        make_reg_symbolic(index);
    }
    else
    {
        make_reg_concrete(index);
    }
}

/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_ld8u_i32(uint64_t t0, uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(is_host_mem_symbolic(addr))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_ld_i32(uint64_t t0, uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(is_host_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_st8_i32(uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(crete_read_was_symbolic)
    {
        make_host_mem_symbolic(addr);
    }
    else
    {
        make_host_mem_concrete(addr);
    }
}

/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_st16_i32(uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(crete_read_was_symbolic)
    {
        make_host_mem_symbolic(addr, addr + 1);
    }
    else
    {
        make_host_mem_concrete(addr, addr + 1);
    }
}

/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_st_i32(uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(crete_read_was_symbolic)
    {
        make_host_mem_symbolic(addr, addr + 1, addr + 2, addr + 3);
    }
    else
    {
        make_host_mem_concrete(addr, addr + 1, addr + 2, addr + 3);
    }
}

void crete_tci_ld8u_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld8u_i32(t0, t1, offset);
}

void crete_tci_ld32u_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld_i32(t0, t1, offset);
}

void crete_tci_ld32s_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld_i32(t0, t1, offset);
}

void crete_tci_ld_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(is_host_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3,
                            addr + 4, addr + 5, addr + 6, addr + 7))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

void crete_tci_st8_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st8_i32(t1, offset);
}

void crete_tci_st16_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st16_i32(t1, offset);
}

void crete_tci_st32_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st_i32(t1, offset);
}

void crete_tci_st_i64(uint64_t t1, uint64_t offset)
{
    uint64_t addr = t1 + offset;
    if(crete_read_was_symbolic)
    {
        make_host_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3,
                               addr + 4, addr + 5, addr + 6, addr + 7);
    }
    else
    {
        make_host_mem_concrete(addr + 0, addr + 1, addr + 2, addr + 3,
                               addr + 4, addr + 5, addr + 6, addr + 7);
    }
}

void crete_tci_qemu_ld8u(uint64_t t0, uint64_t addr)
{
    if(is_guest_mem_symbolic(addr))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

void crete_tci_qemu_ld8s(uint64_t t0, uint64_t addr)
{
    crete_tci_qemu_ld8u(t0, addr);
}

void crete_tci_qemu_ld16u(uint64_t t0, uint64_t addr)
{
    if(is_guest_mem_symbolic(addr + 0, addr + 1))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

void crete_tci_qemu_ld16s(uint64_t t0, uint64_t addr)
{
    crete_tci_qemu_ld16u(t0, addr);
}

void crete_tci_qemu_ld32u(uint64_t t0, uint64_t addr)
{
    if(is_guest_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

void crete_tci_qemu_ld32s(uint64_t t0, uint64_t addr)
{
    crete_tci_qemu_ld32u(t0, addr);
}

void crete_tci_qemu_ld32(uint64_t t0, uint64_t addr)
{
    crete_tci_qemu_ld32u(t0, addr);
}

void crete_tci_qemu_ld64(uint64_t t0, uint64_t addr)
{
    if(is_guest_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3,
                             addr + 4, addr + 5, addr + 6, addr + 7))
    {
        make_reg_symbolic(t0);
    }
    else
    {
        make_reg_concrete(t0);
    }
}

void crete_tci_qemu_ld64_32(uint64_t t0, uint64_t t1, uint64_t addr)
{
    if(is_guest_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3,
                             addr + 4, addr + 5, addr + 6, addr + 7))
    {
        make_reg_symbolic(t0);
        make_reg_symbolic(t1);
    }
    else
    {
        make_reg_concrete(t0);
        make_reg_concrete(t1);
    }
}

void crete_tci_qemu_st8(uint64_t t0, uint64_t addr)
{
    if(is_reg_symbolic(t0))
    {
        make_guest_mem_symbolic(addr);
    }
    else
    {
        make_guest_mem_concrete(addr);
    }
}

void crete_tci_qemu_st16(uint64_t t0, uint64_t addr)
{
    if(is_reg_symbolic(t0))
    {
        make_guest_mem_symbolic(addr + 0, addr + 1);
    }
    else
    {
        make_guest_mem_concrete(addr + 0, addr + 1);
    }
}

void crete_tci_qemu_st32(uint64_t t0, uint64_t addr)
{
    if(is_reg_symbolic(t0))
    {
        make_guest_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3);
    }
    else
    {
        make_guest_mem_concrete(addr + 0, addr + 1, addr + 2, addr + 3);
    }
}

void crete_tci_qemu_st64(uint64_t addr)
{
    if(crete_read_was_symbolic)
    {
        make_guest_mem_symbolic(addr + 0, addr + 1, addr + 2, addr + 3,
                                addr + 4, addr + 5, addr + 6, addr + 7);
    }
    else
    {
        make_guest_mem_concrete(addr + 0, addr + 1, addr + 2, addr + 3,
                                addr + 4, addr + 5, addr + 6, addr + 7);
    }
}

void crete_tci_make_symbolic(uint64_t addr, uint64_t size)
{
    for(uint64_t i = 0; i < size; ++i)
    {
        make_guest_mem_symbolic(addr + i);
    }
}

void crete_tci_next_block()
{
    terminate_block();
}

void crete_tci_mark_block_symbolic()
{
    mark_block_symbolic();
}

bool crete_tci_is_block_branching()
{
    return is_block_branching;
}

void crete_tci_brcond()
{
    mark_block_branching();
}

void crete_tci_next_iteration()
{
    analyzer = Analyzer();
}
