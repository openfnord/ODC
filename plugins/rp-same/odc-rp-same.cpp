/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// STD
#include <iostream>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
// ODC
#include <odc/Version.h>

using namespace std;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try {
        string res;
        std::string partitionID;

        bpo::options_description options("odc-rp-same options");
        options.add_options()("help,h", "Print help");
        options.add_options()("version,v", "Print version");
        options.add_options()("id", boost::program_options::value<std::string>(&partitionID)->default_value(""), "Partition ID");
        options.add_options()("res", bpo::value<string>(&res)->default_value(""), "Resource description");

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            cout << options;
            return EXIT_SUCCESS;
        }

        if (vm.count("version")) {
            cout << ODC_VERSION << endl;
            return EXIT_SUCCESS;
        }

        cout << res;
    } catch (exception& _e) {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
