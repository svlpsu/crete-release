/***
 * Author: Christopher Havlicek
 */

#ifndef CRETE_EXCEPTION_H
#define CRETE_EXCEPTION_H

#include <exception>
#include <stdexcept>
#include <string>

#include <boost/exception/all.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem.hpp>

#include <sys/types.h>

namespace crete
{

namespace err
{
    // General
    typedef boost::error_info<struct tag_msg, std::string> msg;
    typedef boost::error_info<struct tag_invalid, std::string> invalid;
    typedef boost::error_info<struct tag_c_errno, int> c_errno;
    // File
    typedef boost::error_info<struct tag_file, std::string> file;
    typedef boost::error_info<struct tag_file_missing, std::string> file_missing;
    typedef boost::error_info<struct tag_file_exists, std::string> file_exists;
    typedef boost::error_info<struct tag_file_open_failed, std::string> file_open_failed;
    typedef boost::error_info<struct tag_file_create, std::string> file_create;
    // Process
    typedef boost::error_info<struct tag_process, std::string> process;
    typedef boost::error_info<struct tag_process_exit_status, std::string> process_exit_status;
    typedef boost::error_info<struct tag_process_not_exited, std::string> process_not_exited;
    typedef boost::error_info<struct tag_process_exited, std::string> process_exited;
    typedef boost::error_info<struct tag_process_error, pid_t> process_error;
    // ???
    typedef boost::error_info<struct tag_obj_not_available, std::string> obj_not_available;
    // Arguments
    typedef boost::error_info<struct tag_arg_invalid_uint, size_t> arg_invalid_uint;
    typedef boost::error_info<struct tag_arg_invalid_str, std::string> arg_invalid_str;
    typedef boost::error_info<struct tag_arg_count, size_t> arg_count;
    typedef boost::error_info<struct tag_arg_missing, std::string> arg_missing;
    // Environment
    typedef boost::error_info<struct tag_invalid_env, std::string> invalid_env;
    // Parsing
    typedef boost::error_info<struct tag_parse, std::string> parse;
    // Network
    typedef boost::error_info<struct tag_network, std::string> network;
    typedef boost::error_info<struct tag_network_mismatch, uint32_t> network_type;
    typedef boost::error_info<struct tag_network_mismatch, uint32_t> network_type_mismatch; // TODO: should include uint32_t,uint32_t to show the mismatch.
    // Mode
    typedef boost::error_info<struct tag_mode, std::string> mode;
} // namespace err

struct Exception : virtual boost::exception, virtual std::exception {};

namespace exception
{

inline
void file_exists(boost::filesystem::path p)
{
    if(!boost::filesystem::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{p.string()});
    }
}

} // namespace exception
} // namespace crete

#endif // CRETE_EXCEPTION_H
