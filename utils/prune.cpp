#include "network.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

void usage(char **argv)
{
    std::cerr << "CASPIAN Network Prune Utility (using Native Prune)" << std::endl;
    std::cerr << "Usage: " << argv[0] << " network_file prune_io(0|1)" << std::endl;
    exit(0);
}

int main(int argc, char **argv)
{
    std::stringstream ss;
    std::string nfilename;
    bool prune_io = false;

    if(argc < 2)
    {
	usage(argv);
    }

    ss.str(argv[1]);
    ss >> nfilename;

    if(argc >= 3)
    {
        prune_io = atoi(argv[2]);
    }

    std::cerr << "Prune I/O Neurons: " << ((prune_io) ? "true" : "false") << std::endl; 

    std::ifstream net_fstream(nfilename);

    caspian::Network net;
    net.from_stream(net_fstream);
    net.prune(prune_io);

    net.to_stream(std::cout);
    std::cout << std::endl;

    return 0;
}

/* vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab: */
