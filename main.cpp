#include "my-router.h"
#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>

int main(int argc, char* argv[])
{
	if (argc != 2 || strlen(argv[1]) != 1 
		|| !valid_router_id(*(argv[1])))
    {
      std::cerr << "Usage: my-router <RouterID>\n";
      return 1;
    }

    boost::asio::io_service io_service;
    DVRouter B = DVRouter(*(argv[1]), io_service);
    io_service.run();
    //B.ft_print();
    //B.dv_print();
}