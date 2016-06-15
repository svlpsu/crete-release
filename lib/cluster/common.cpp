#include <crete/cluster/common.h>
#include <crete/exception.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/uuid/uuid_generators.hpp>

//#include <boost/algorithm/string/join.hpp>

#include <boost/process.hpp>

#include <iostream> // testing.

namespace fs = boost::filesystem;
namespace bp = boost::process;
namespace bui = boost::uuids;

namespace crete
{
namespace cluster
{

auto from_trace_file(const fs::path& p) -> Trace
{
    if(!fs::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{p.string()});
    }

    auto trace = Trace{};

    trace.uuid_ = bui::random_generator{}();

    auto newp = p.parent_path() / bui::to_string(trace.uuid_);

    fs::rename(p,
               newp);

    auto tared_path = fs::path{newp}.replace_extension("tar.gz");

    bp::context ctx;
    ctx.work_directory = newp.parent_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = std::string{"/bin/tar"};
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-zcf",
                                         tared_path.filename().string(),
                                         newp.filename().string()
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    if(status.exit_status() != 0)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
    }

    {
        fs::ifstream ifs{tared_path};

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{tared_path.string()});
        }

        std::copy(std::istreambuf_iterator<char>{ifs},
                  std::istreambuf_iterator<char>{},
                  std::back_inserter(trace.data_));
    }

    fs::remove_all(newp);
    fs::remove(tared_path);

    return trace;
}

auto to_file(const Trace& trace,
             const boost::filesystem::path& p) -> void
{
    auto tared_path = fs::path{p}.replace_extension("tar");

    if(fs::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_exists{p.string()});
    }

    {
        fs::ofstream ofs{tared_path};

        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{tared_path.string()});
        }

        std::copy(trace.data_.begin(),
                  trace.data_.end(),
                  std::ostreambuf_iterator<char>{ofs});
    }

    bp::context ctx;
    ctx.work_directory = p.parent_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = std::string{"/bin/tar"}; // TODO: get from fs::find_in_path("tar"); ...
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-xf",
                                         tared_path.filename().string()
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    if(status.exit_status() != 0)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
    }

    fs::remove(tared_path);
}

ImageInfo::ImageInfo(const boost::filesystem::path& image) :
    file_name_{image.filename().string()},
    last_write_time_{fs::last_write_time(image)}
{
}

auto from_image_file(const boost::filesystem::path& p) -> OSImage
{
    if(!fs::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{p.string()});
    }

    auto image = OSImage{};

    auto tared_path = fs::path{p}.replace_extension(".tar.gz");

    bp::context ctx;
    ctx.work_directory = fs::current_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = std::string{"/bin/tar"}; // TODO: these should all use 'find_in_path()' instead of abs path.
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-zcf",
                                         tared_path.filename().string(),
                                         p.filename().string()
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    if(status.exit_status() != 0)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
    }

    {
        fs::ifstream ifs{tared_path};

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{tared_path.string()});
        }

        std::copy(std::istreambuf_iterator<char>{ifs},
                  std::istreambuf_iterator<char>{},
                  std::back_inserter(image.data_));
    }

    fs::remove(tared_path);

    return image;
}

auto to_file(const OSImage& image,
             const boost::filesystem::path& p) -> void
{
    auto tared_path = fs::path{p}.replace_extension("tar");

    if(fs::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_exists{p.string()});
    }

    {
        fs::ofstream ofs{tared_path};

        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{tared_path.string()});
        }

        std::copy(image.data_.begin(),
                  image.data_.end(),
                  std::ostreambuf_iterator<char>{ofs});
    }

    bp::context ctx;
    ctx.work_directory = tared_path.parent_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = std::string{"/bin/tar"}; // TODO: get from fs::find_in_path("tar"); ...
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-xf",
                                         tared_path.filename().string()
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    if(status.exit_status() != 0)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
    }

    fs::remove(tared_path);
}

} // namespace cluster
} // namespace crete
