#include "my-router.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream> 
#include <climits>
#include <cstring>
#include <cctype>

using namespace std;
using boost::asio::ip::udp;

DVRouter::DVRouter(char rid, boost::asio::io_service& io_service)
    : id(rid), port(port_no(rid)), 
    socket(io_service, udp::endpoint(udp::v4(), port))
{
    bzero(data_buffer, MAX_LENGTH);
	ft_init(); dv_init();
    start_receive();
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
      if(type == DATA_PKT)
        printf("Data packet received\n");
    else if(type == CONTROL_PKT)
        printf("Control packet received\n");
    else
        printf("Invalid packet received\n");

      start_receive();
    }
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
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            if (line[0]!=id) // find the matching rows!
                {continue;}
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

