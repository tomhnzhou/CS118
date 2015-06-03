#ifndef MY_ROUTER_H
#define MY_ROUTER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string>
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
    bool alive;
    int realcost() {return alive?cost:INT_MAX;} //what's the real cost?
} FTEntry;

typedef struct{
	time_t time_sent;	//in POSIX seconds
	int pkt_id;
	char dest_id;
} DataRecordEntry;

enum PKT_TYPE {
	INVALID_PKT,
	DATA_PKT,
	CONTROL_PKT, 
	RREP_PKT
};

class DVRouter
{
public:
	DVRouter(char rid, boost::asio::io_service& io_service); // initializer
	void ft_init();   // initialize forwarding table
    void dv_init();   // initialize distance vector table, need to ft_init first
    void ft_print(char* print_str);  // print the forwarding table
    void dv_print(char* print_str);  // print the distance vector table
    bool update(int dv[6], char neighbor_id);
    void mark_dead_router(char rid);
	void start_receive();
	void handle_receive(const boost::system::error_code& error,
  						std::size_t);
	void handle_send(boost::shared_ptr<std::string> /*message*/,
      const boost::system::error_code& /*error*/,
      std::size_t /*bytes_transferred*/);
	void handle_data_pkt();
	void handle_control_pkt();
	void handle_rrep_pkt();

	int get_out_port(char dest);
	void send_to(int port);
	void log_received_data(bool print_dv);
	void log_error(char* err_msg);
	void write_log(char* log_str);

	PKT_TYPE get_packet_type();
	void periodic_send();
	void handle_timeout();
	void prepare_dv_send(char);
	void prepare_rrep_send();
	bool is_neighbor(char rid);
	bool dv_changed(int old_dv[6][6], int new_dv[6][6]);

	int parse_msg(char* buf, std::string& line);
	void parse_dv_line(std::string line, int dv[6]);


	char id;          // my id (A ~ F)
	int port;      // my port number
	udp::socket socket;     // my socket descriptor
	udp::endpoint sender_endpoint;
	boost::asio::deadline_timer* periodic_timer;
	boost::asio::deadline_timer* timeout_timer;
	const static int MAX_LENGTH = 1024;
	char data_buffer[MAX_LENGTH];

	int dv_rcvd[6];
	int last_pkt_id;
	DataRecordEntry data_record[16];
    char neighbors[6]; // who are my neighbors?
	FTEntry ft[6];    // the forwarding table
    int DV[6][6];     // the distance vector table

};

bool valid_router_id(char id);
int port_no(char id);



#endif /* MY_ROUTER_H */