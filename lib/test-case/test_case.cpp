#include <crete/test_case.h>

#include <cassert>
#include <iomanip>
#include <stdexcept>

using namespace std;

namespace crete
{
    ostream& operator<<(ostream& os, const TestCaseElement& elem)
    {
        os << "name.size: " << elem.name_size << endl;
        os << "name: " << string(elem.name.begin(), elem.name.end()) << endl;
        os << "data.size: " << elem.data_size << endl;
        os << "data: " << string(elem.data.begin(), elem.data.end()) << endl;
        os << "hex:" << hex;
        for(vector<uint8_t>::const_iterator iter = elem.data.begin();
            iter != elem.data.end();
            ++iter)
            os << " " << setw(2) << setfill('0') << uint32_t(*iter);
        os << dec << endl;

        return os;
    }

    ostream& operator<<(ostream& os, const crete::TestCase& tc)
    {
        for(vector<TestCaseElement>::const_iterator iter = tc.elems_.begin(); iter != tc.elems_.end(); ++iter)
        {
            os << "element #" << distance(tc.elems_.begin(), iter) << endl;
            os << *iter;
        }

        return os;
    }

    size_t stream_size(istream& is)
    {
        size_t current = is.tellg();
        is.seekg(0, std::ios::end);
        size_t size = is.tellg();
        is.seekg(current, std::ios::beg);

        return size;
    }

    void write(ostream& os, const TestCaseElement& elem)
    {
        os.write(reinterpret_cast<const char*>(&elem.name_size), sizeof(uint32_t));
        copy(elem.name.begin(), elem.name.end(), ostream_iterator<uint8_t>(os));
        os.write(reinterpret_cast<const char*>(&elem.data_size), sizeof(uint32_t));
        copy(elem.data.begin(), elem.data.end(), ostream_iterator<uint8_t>(os));
    }

    void write(ostream& os, const vector<TestCaseElement>& elems)
    {
        uint32_t elem_count = elems.size();
        os.write(reinterpret_cast<const char*>(&elem_count), sizeof(uint32_t));
        for(vector<TestCaseElement>::const_iterator iter = elems.begin(); iter != elems.end(); ++iter)
        {
            crete::write(os, *iter);
        }
    }


    void write(std::ostream& os, const std::vector<TestCase>& tcs)
    {
        uint32_t tc_count = tcs.size();
        os.write(reinterpret_cast<const char*>(&tc_count), sizeof(uint32_t));
        for(vector<TestCase>::const_iterator iter = tcs.begin(); iter != tcs.end(); ++iter)
        {
            iter->write(os);
        }
    }

    TestCase::TestCase() :
        priority_(0)
    {
    }

    void TestCase::write(ostream& os) const
    {
        crete::write(os, elems_);
    }

    TestCaseElement read_test_case_element(istream& is)
    {
        TestCaseElement elem;

        size_t ssize = stream_size(is);

        if(ssize > 1000000)
        {
            throw std::runtime_error("Sanity check: test case file size is unexpectedly large (size > 1000000).");
        }

        is.read(reinterpret_cast<char*>(&elem.name_size), sizeof(uint32_t));
        assert(elem.name_size < ssize);

        if(elem.name_size > 1000)
        {
            throw std::runtime_error("Sanity check: test case element name size is unexpectedly large (size > 1000).");
        }

        elem.name.resize(elem.name_size);
        is.read(reinterpret_cast<char*>(elem.name.data()), elem.name_size);


        is.read(reinterpret_cast<char*>(&elem.data_size), sizeof(uint32_t));
        assert(elem.data_size < ssize);

        if(elem.data_size > 1000000)
        {
            throw std::runtime_error("Sanity check: test case element name size is unexpectedly large (size > 1000000).");
        }

        elem.data.resize(elem.data_size); // TODO: inefficient. Resize initializes all values.
        is.read(reinterpret_cast<char*>(elem.data.data()), elem.data_size);

        return elem;
    }

    TestCase read_test_case(istream& is)
    {
        TestCase tc;

        size_t ssize = stream_size(is);

        if(ssize > 1000000)
        {
            throw std::runtime_error("Sanity check: test case file size is unexpectedly large (size > 1000000).");
        }

        uint32_t elem_count;
        is.read(reinterpret_cast<char*>(&elem_count), sizeof(uint32_t));

        assert(elem_count != 0 &&
               elem_count < (ssize / elem_count));

        for(uint32_t i = 0; i < elem_count; ++i)
        {
            tc.add_element(read_test_case_element(is));
        }

        return tc;
    }

    vector<TestCase> read_test_cases(istream& is)
    {
        vector<TestCase> tcs;

        size_t ssize = stream_size(is);

        uint32_t tc_count;
        is.read(reinterpret_cast<char*>(&tc_count), sizeof(uint32_t));

        assert(tc_count != 0 &&
               tc_count < (ssize / tc_count));

        for(uint32_t i = 0; i < tc_count; ++i)
        {
            tcs.push_back(read_test_case(is));
        }

        return tcs;
    }
}
