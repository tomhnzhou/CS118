#ifndef MY_ROUTER_H
#define MY_ROUTER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::udp;
 

typedef struct{
	char dest_id;
	int cost;
	int out_port;
	int dest_port;
} FTEntry;

enum PKT_TYPE {
	INVALID_PKT,
	DATA_PKT,
	CONTROL_PKT
};

class DVRouter
{
public:
	DVRouter(char rid, boost::asio::io_service& io_service); // initializer
	void ft_init();   // initialize forwarding table
    void dv_init();   // initialize distance vector table, need to ft_init first
    void ft_print();  // print the forwarding table
    void dv_print();  // print the distance vector table
	void start_receive();
	void handle_receive(const boost::system::error_code& error,
  						std::size_t);
	void handle_send(boost::shared_ptr<std::string> /*message*/,
      const boost::system::error_code& /*error*/,
      std::size_t /*bytes_transferred*/);
	void handle_data_pkt();
	void handle_control_pkt();

	PKT_TYPE get_packet_type();
	void periodic_send();
	void format_dv_update();


	char id;          // my id (A ~ F)
	int port;      // my port number
	udp::socket socket;     // my socket descriptor
	udp::endpoint sender_endpoint;
	boost::asio::deadline_timer* timer;
	const static int MAX_LENGTH = 1024;
	char data_buffer[MAX_LENGTH];

    char neighbors[6]; // who are my neighbors?
	FTEntry ft[6];    // the forwarding table
    int DV[6][6];     // the distance vector table

};

bool valid_router_id(char id);
int port_no(char id);



#endif /* MY_ROUTER_H */