#include "my-router.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream> 
#include <climits>
#include <cstring>
#include <cctype>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/ref.hpp>

using namespace std;
using boost::asio::ip::udp;


DVRouter::DVRouter(char rid, boost::asio::io_service& io_service)
    : id(rid), port(port_no(rid)), 
    socket(io_service, udp::endpoint(udp::v4(), port))
{
    timer = new boost::asio::deadline_timer(io_service, boost::posix_time::seconds(5));
    bzero(data_buffer, MAX_LENGTH);
    bzero(neighbors, 6);
	ft_init(); dv_init();
    start_receive();
    timer->async_wait(boost::bind(&DVRouter::periodic_send, this));
}

// periodically send a message to each neighbor every 5 seconds
void DVRouter::periodic_send()
{
	std::cout << "Another 5 seconds...\n";

	for(int i = 0; neighbors[i] != 0 && i < 6; i++){
		int neighbor_port = port_no(neighbors[i]);
		format_dv_update();
		boost::shared_ptr<std::string> message(
          new std::string(data_buffer));
		socket.async_send_to(boost::asio::buffer(*message), 
						udp::endpoint(udp::v4(), neighbor_port),
			          boost::bind(&DVRouter::handle_send, this, message,
			            boost::asio::placeholders::error,
			            boost::asio::placeholders::bytes_transferred));
	}

	timer->expires_at(timer->expires_at() + boost::posix_time::seconds(5));
	timer->async_wait(boost::bind(&DVRouter::periodic_send, this));
}


void DVRouter::start_receive()
{
    socket.async_receive_from(
    boost::asio::buffer(data_buffer), sender_endpoint,
    boost::bind(&DVRouter::handle_receive, this,
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred));
}

void DVRouter::handle_receive(const boost::system::error_code& error,
  std::size_t /*bytes_transferred*/)
{
    if (!error || error == boost::asio::error::message_size)
    {
        printf("Server Received Message from port %d:\n%s\n", 
                sender_endpoint.port(), data_buffer);

        PKT_TYPE type = get_packet_type();
        if(type == DATA_PKT){
            printf("Data packet received\n");
            handle_data_pkt();
          }
        else if(type == CONTROL_PKT){
            printf("Control packet received\n");
            handle_control_pkt();
          }
        else
            printf("Invalid packet received\n");

        start_receive();
    }
}

void DVRouter::handle_send(boost::shared_ptr<std::string> /*message*/,
      const boost::system::error_code& /*error*/,
      std::size_t /*bytes_transferred*/)
{
}

void DVRouter::handle_data_pkt()
{

}

void DVRouter::handle_control_pkt()
{

}

//format a DV update message and store it in data_buffer
void DVRouter::format_dv_update()
{
	bzero(data_buffer, MAX_LENGTH);
	strcpy(data_buffer, "Type: Control\n");

	char source_id[11]; bzero(source_id, 11);
	strcpy(source_id, "Src ID: ");
	source_id[8] = id;
	source_id[9] = '\n';
	strcat(data_buffer, source_id);

	char self_dv[12]; bzero(self_dv, 12);

	int row_num = id - 'A';
	for(int i=0; i<11; i++){
		if(i % 2)	//i is odd
			self_dv[i] = ',';
		else
			self_dv[i] = 
				(DV[row_num][i/2] == INT_MAX) ? '-' : ('0'+DV[row_num][i/2]);
	}
	strcat(data_buffer, self_dv);
}

PKT_TYPE DVRouter::get_packet_type(){
    char* found;
    if( !(found = strstr(data_buffer, "Type: ")) )
        return INVALID_PKT;

    found += 6;
    char type_str[10];
    PKT_TYPE type;
    bzero(type_str, 10);

    for(int i = 0; found[i] != 0 && !isspace(found[i]); i++)
        type_str[i] = found[i];
    if(!strcmp(type_str, "Control")) type = CONTROL_PKT;
    else if(!strcmp(type_str, "Data")) type = DATA_PKT;
    else type = INVALID_PKT;

    return type;
}



void DVRouter::ft_init()  // initialize forwarding table
{
    string line;
    for (int i = 0; i < 6; i++)
        {
            ft[i].dest_id = 'A' + i;
            ft[i].cost = INT_MAX;
            if (ft[i].dest_id == id)
                {ft[i].cost = 0;}
            ft[i].out_port = port;
            ft[i].dest_port = 0;
        }
    ifstream myfile ("topology.txt", ifstream::in); // change topology file here!!!!

    int neighbor_i = 0;
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            if (line[0]!=id) // find the matching rows!
                {continue;}
            neighbors[neighbor_i] = line[2];
            neighbor_i++;
            int row_num = line[2] - 'A'; //find the right row to initialize
            line.erase(0,4);

            string delimiter = ",";
            string token = line.substr(0, line.find(delimiter));
            ft[row_num].dest_port = stoi(token, nullptr, 10);

            line.erase(0, line.find(delimiter) + delimiter.length());
            token = line.substr(0, line.find(delimiter));
            ft[row_num].cost = min(stoi(token, nullptr, 10), ft[row_num].cost);
        }
        myfile.close();
    } 
}

void DVRouter::dv_init()  // initialize distance vector table
{
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            DV[i][j] = INT_MAX;
        }
    }
    int id_num = id - 'A';
    for (int i = 0; i < 6; i++)
    {
        int j = ft[i].dest_id - 'A';
        DV[id_num][j] = min(DV[id_num][j], ft[i].cost);
    }
}

void DVRouter::ft_print() // print the forwarding table
{
    printf("\nprinting FT... 0______0\n");
    for (int i = 0; i < 6; i++)
    { printf("%c | C=%d, outport=%d, destport=%d\n", ft[i].dest_id, ft[i].cost, ft[i].out_port, ft[i].dest_port);}
    printf("end of FT!\n");
}

void DVRouter::dv_print() // print the distance vector table
{
    printf("printing DV Table~\n    ");
    for (int j = 0; j < 6; j++)
        {printf("%c | ", 'A'+j);}
    printf("\n");
    for (int i = 0; i < 6; i++)
    {
        printf("%c | ", 'A'+i);
        for (int j = 0; j < 6; j++)
            {printf("%d | ", DV[i][j]);}
        printf("\n");
    }
}

bool valid_router_id(char id){
    return id == 'A' || id == 'B' || id == 'C'
        || id == 'D' || id == 'E' || id == 'F';
}

int port_no(char id){
    if(valid_router_id(id))
        return 10000 + id - 'A';
    return -1;
}

