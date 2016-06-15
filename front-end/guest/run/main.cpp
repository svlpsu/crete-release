#include "runner.h"

#include <iostream>

#include <crete/exception.h>

using namespace std;
using namespace crete;

int main(int argc, char* argv[])
{
    try
    {
//        fs::path config_path(argv[1]);

//        if(!fs::exists(config_path))
//            throw runtime_error("configuration file not found: " + config_path.generic_string());

//        pt::ptree config_tree;

//        pt::read_xml(config_path.generic_string(), config_tree);

//        Runner runner(config_tree);
        Runner runner(argc, argv);

    }
    catch(crete::Exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
    }
    catch(std::exception& e)
    {
        cout << "[std] Exception: " << e.what() << endl;
    }
    catch(...)
    {
        cout << "Exception: unknown" << endl;
    }

	return 0;
}
