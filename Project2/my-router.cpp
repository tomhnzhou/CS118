#include "my-router.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
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
    if(id == 'H'){
        bzero(data_buffer, MAX_LENGTH);
        strcpy(data_buffer, "Type: Data\n");
        strcat(data_buffer, "Src ID: A, Dest ID: D\n");
        strcat(data_buffer, "Path:       \n");
        strcat(data_buffer, "Pkt ID: \n");

        int header_len = strlen(data_buffer);
        int bytes_to_read = MAX_LENGTH - header_len -1;
        int fd = open("testdata.txt", O_RDONLY);
        char* sof = &data_buffer[header_len];
        if(read(fd, sof, bytes_to_read) < 0) return;

        send_to(port_no('A'));
    }
    else {
        periodic_timer = new boost::asio::deadline_timer(io_service, 
                        boost::posix_time::seconds(5));
        timeout_timer = new boost::asio::deadline_timer(io_service, 
                        boost::posix_time::seconds(10));
        bzero(data_buffer, MAX_LENGTH);
        bzero(neighbors, 6);
        memset(dv_rcvd, 0, 6*sizeof(int));
        last_pkt_id = 0;
        for(int i=0; i<16; i++) 
            data_record[i].pkt_id = -1;
    	ft_init(); dv_init(); log_init();//dv_print(); ft_print();
        start_receive();
        periodic_timer->async_wait(boost::bind(&DVRouter::periodic_send, this));
        timeout_timer->async_wait(boost::bind(&DVRouter::handle_timeout, this));
    }
}

// periodically send a message to each neighbor every 5 seconds
void DVRouter::periodic_send()
{
	//std::cout << "Another 5 seconds...\n";

	for(int i = 0; neighbors[i] != 0 && i < 6; i++){
		int neighbor_port = port_no(neighbors[i]);
		prepare_dv_send(neighbors[i]);
        send_to(neighbor_port);
        bzero(data_buffer, MAX_LENGTH);
	}

	periodic_timer->expires_at(periodic_timer->expires_at() + boost::posix_time::seconds(5));
	periodic_timer->async_wait(boost::bind(&DVRouter::periodic_send, this));
}

void DVRouter::handle_timeout()
{
        //std::cout << "Another 10 seconds...\n";
    for(int i = 0; i < 6; i++){
        if(dv_rcvd[i] == 0 && is_neighbor('A'+i)){
            mark_dead_router('A'+i);
            char err_msg[128]; bzero(err_msg,128);
            sprintf(err_msg, 
                "Have not received Control message from %c for a long time.\n", 
                'A'+i);
            log_error(err_msg);
        }
    }
    memset(dv_rcvd, 0, 6*sizeof(int));

    for(int i = 0; i < 16; i++){
        if(data_record[i].pkt_id != -1){
            double sec = difftime(time(0), data_record[i].time_sent);
            if(sec > 5){
                //mark_dead_router(data_record[i].dest_id);
                data_record[i].pkt_id = -1;
                char err_msg[128]; bzero(err_msg,128);
                sprintf(err_msg, 
                    "Have not received RREP from %c for a long time.\n", 
                    data_record[i].dest_id);
                log_error(err_msg);
            }
        }
    }

    timeout_timer->expires_at(timeout_timer->expires_at() + boost::posix_time::seconds(10));
    timeout_timer->async_wait(boost::bind(&DVRouter::handle_timeout, this));
}

bool DVRouter::is_neighbor(char rid){
    return strchr(neighbors, rid);
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
    if (!error)
    {
        PKT_TYPE type = get_packet_type();
        if(type == DATA_PKT){
            //printf("Data packet received\n");
            handle_data_pkt();
          } else if(type == CONTROL_PKT){
            //printf("Control packet received\n");
            handle_control_pkt();
          } else if(type == RREP_PKT) {
            handle_rrep_pkt();
          } 
          else{
            printf("Invalid packet received\n");
        }
        bzero(data_buffer, MAX_LENGTH);
        start_receive();
    }
}

void DVRouter::handle_send(boost::shared_ptr<std::string> /*message*/,
      const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/)
{
}

//set offset until next newline, return line as string
int DVRouter::parse_msg(char* buf, string& line)
{
    char* ptr = strstr(buf, "\n");
    if(!ptr){
        line = string(buf);
        return strlen(buf);
    }
    int offset = ptr - data_buffer + 1;
    line = string(buf, ptr-buf);
    return offset;
}

void DVRouter::parse_dv_line(string line, int dv[6])
{
    // cout<<line;
    string delimiter = ",";
    string token;
    for(int i=0; i<5; i++){
        token = line.substr(0, line.find(delimiter));
        //printf("Token is: %s\n", token.c_str());
        if(token == "-")
            dv[i] = INT_MAX;
        else{
            //cout << id << "> Token is: " << token << endl;
            dv[i] = stoi(token, nullptr, 10);
        }

        line.erase(0, line.find(delimiter) + delimiter.length());
        //printf("After erase, line is: %s\n", line.c_str());
    }
    if(line == "-")
        dv[5] = INT_MAX;
    else{
        //cout << id << "> Line is: " << line << endl;
        dv[5] = stoi(line, nullptr, 10);
    }
}

void DVRouter::handle_control_pkt()
{
    int offset;
    string line;
    char* next_line;

    // "Type: " line
    if((offset = parse_msg(data_buffer, line)) < 0) return;

    // "Src ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    char sender_id = line[8];
    dv_rcvd[sender_id - 'A'] = 1;
    //printf("Sender ID is: %c\n", sender_id);

    // DV line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    int dv[6];
    parse_dv_line(line, dv);

    char path[3]; 
    path[0] = sender_id; path[1] = id; path[2]=0;

    //printf("Parsed DV: %d, %d, %d, %d, %d, %d\n", 
            //dv[0], dv[1], dv[2], dv[3], dv[4], dv[5]);

    // if received ctrl packet from sender, sender is alive!
    ft[sender_id-'A'].alive = true;

    bool dv_changed = update(dv, sender_id);
    log_received_data(dv_changed);
    //dv_print(); ft_print();
    //write_output_file(CONTROL_PKT, sender_id, id, 
                //port_no(sender_id), -1, string(path));
}

void DVRouter::handle_data_pkt()
{
    int offset;
    string line;
    char* next_line;

    // "Type: " line
    if((offset = parse_msg(data_buffer, line)) < 0) return;

    // "Src ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    char src_id = line[8];
    char dest_id = line[20];

    // "Path: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    int path_len = 0;
    for(int i = 6; i < line.length(); i++){
        if(valid_router_id(line[i])) path_len++;
        else break;
    }
    next_line[6+path_len] = id;
    string path = line.substr(6, path_len+1);

    // "Pkt ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    string pkt_id;
    if(src_id == id){
        //printf("%c > Buffer before adding path: %s\n", id, data_buffer);
        last_pkt_id++;
        pkt_id = to_string(last_pkt_id);
        int bytes_to_move = strlen(data_buffer) - offset + 1;
        char* move_src = &data_buffer[offset-1];
        //printf("%c > Src move: %s\n", id, move_src);
        char* move_dest = move_src + pkt_id.length();
        memmove(move_dest, move_src, bytes_to_move);
        strcpy(move_src, pkt_id.c_str());
        move_src[pkt_id.length()] = '\n';
        //printf("%c > Buffer after adding path: %s\n", id,data_buffer);
    } else {
        int i;
        for(i = 8; i < MAX_LENGTH; i++){
            if(isspace(next_line[i])) 
                break;
        }
        pkt_id = string(&next_line[8], i-8);
        //printf("%c > Parsed pkt id is: %s\n", id, pkt_id.c_str());
    }

    //printf("Sender ID is: %c\n", sender_id);
    int out_port = get_out_port(dest_id);
    log_received_data(false);
    
    if(dest_id == id) {
        prepare_rrep_send();
        
        out_port = get_out_port(src_id);
    } else if (src_id == id){
        for(int i = 0; i < 16; i++){
            if(data_record[i].pkt_id <= 0){
                data_record[i].pkt_id = last_pkt_id;
                data_record[i].time_sent = time(0);
                data_record[i].dest_id = dest_id;
                break;
            }
        }
    }

    send_to(out_port);
}

void DVRouter::handle_rrep_pkt()
{
    //printf("%c > RREP received: \n%s\n", id, data_buffer);

    int offset;
    string line;
    char* next_line;

    // "Type: " line
    if((offset = parse_msg(data_buffer, line)) < 0) return;

    // "Src ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    char dest_id = line[20];

    // if dest of rrep is not reached, 
    //  treat it like any data pkt
    if(dest_id != id){
        handle_data_pkt();
        return;
    }

    // "Path: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    // "Pkt ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    int i;
    for(i = 8; i < MAX_LENGTH; i++){
        if(isspace(next_line[i])) 
            break;
    }
    string pkt_id_str = string(&next_line[8], i-8);
    int rcvd_pkt_id = stoi(pkt_id_str);

    for(i=0; i<16; i++){
        if(data_record[i].pkt_id == rcvd_pkt_id){
            data_record[i].pkt_id = -1;
            break;
        }
    }

    log_received_data(false);
}
// send the data in data_buffer to the specified port
void DVRouter::send_to(int port){
    char neighbor_id = 'A' + (port-10000);
    boost::shared_ptr<std::string> message(
            new std::string(data_buffer));
    socket.async_send_to(boost::asio::buffer(*message), 
                        udp::endpoint(udp::v4(), port),
                        boost::bind(&DVRouter::handle_send, this, message,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    //bzero(data_buffer, MAX_LENGTH);
}

void DVRouter::log_received_data(bool print_dv)
{
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char* time_str = asctime(timeinfo);
    time_str[strlen(time_str)-1] = 0;

    char log_line[1024]; bzero(log_line, 1024);
    sprintf (log_line, 
        "/////////////////////////////////\n"
        "Router %c> %s \n"
        "/////////////////////////////////\n\n"
        "Received Message from port %d:\n"
        "+------------------------+\n%s\n"
        "+------------------------+\n",
            id, time_str, sender_endpoint.port(), data_buffer);

    if(print_dv){
        strcat(log_line, "Updated DV and Forwarding Table: \n\n");
        int log_len = strlen(log_line);
        ft_print(&log_line[log_len]); 
        log_len = strlen(log_line);
        dv_print(&log_line[log_len]);
    }
    else
        strcat(log_line, "DV and Forwarding Table unchanged.\n\n");
    write_log(log_line);
}

void DVRouter::log_error(char* err_msg)
{
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char* time_str = asctime(timeinfo);
    time_str[strlen(time_str)-1] = 0;

    char log_line[1024]; bzero(log_line, 1024);
    sprintf (log_line, 
        "/////////////////////////////////\n"
        "Router %c> %s \n"
        "/////////////////////////////////\n\n"
        "Error: %s\n",
            id, time_str, err_msg);

    strcat(log_line, "Updated DV and Forwarding Table: \n\n");
    int log_len = strlen(log_line);
    ft_print(&log_line[log_len]); 
    log_len = strlen(log_line);
    dv_print(&log_line[log_len]);

    write_log(log_line);
}

void DVRouter::log_init()
{
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char* time_str = asctime(timeinfo);
    time_str[strlen(time_str)-1] = 0;

    char log_line[1024]; bzero(log_line, 1024);
    sprintf (log_line, 
        "/////////////////////////////////\n"
        "Router %c> %s \n"
        "/////////////////////////////////\n\n"
        "Finished Router Initialization\n",
            id, time_str);

    strcat(log_line, "Initialized DV and Forwarding Table: \n\n");
    int log_len = strlen(log_line);
    ft_print(&log_line[log_len]); 
    log_len = strlen(log_line);
    dv_print(&log_line[log_len]);
    
    write_log(log_line);
}

void DVRouter::write_log(char* log_str)
{
    int fd;
    char log_filename[32]; bzero(log_filename, 32);
    sprintf(log_filename, "routing-output%c.txt", id);
    if((fd = open(log_filename, O_RDWR | O_APPEND | O_CREAT, 0644)) < 0) {
        printf("%c > Error: open log file failed, %s\n", 
                id, strerror(errno));
        return;
    }

    if(write(fd, log_str, strlen(log_str)) < 0)
        printf("%c > Error: failed write to file, %s\n", 
                id, strerror(errno));

    close(fd);
}

// returns the next port on the shrotest path to dest
int DVRouter::get_out_port(char dest)
{
    int i = dest-'A';
    return ft[i].dest_port;
}

//format a DV update message and store it in data_buffer
void DVRouter::prepare_dv_send(char dv_dest_id)
{
	bzero(data_buffer, MAX_LENGTH);
	strcpy(data_buffer, "Type: Control\n");

	char source_id[11]; bzero(source_id, 11);
	strcpy(source_id, "Src ID: ");
	source_id[8] = id;
	source_id[9] = '\n';
	strcat(data_buffer, source_id);

	char self_dv[64]; bzero(self_dv, 64);

	int row_num = id - 'A';
    string element;
	for(int i=0; i<6; i++){
		if(DV[row_num][i] == INT_MAX
            || ft[i].dest_id == dv_dest_id)
            strcat(self_dv, "-");
        else{
			element = to_string(DV[row_num][i]);
            strcat(self_dv, element.c_str());
        }
        if(i == 5) break;
        strcat(self_dv, ",");
	}
    //cout<<data_buffer;
	strcat(data_buffer, self_dv);
}

void DVRouter::prepare_rrep_send()
{
    int offset;
    string line;
    char* next_line;

    // "Type: " line
    if((offset = parse_msg(data_buffer, line)) < 0) return;
    strcpy(&data_buffer[6], "RREP");
    data_buffer[10] = '\n';

    // "Src ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    char src_id = line[8];
    char dest_id = line[20];
    next_line[20] = src_id; next_line[8] = dest_id;

    // "Path: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    int path_len = 0;
    next_line[6] = id;
    for(int i = 7; i < 12; i++)
        next_line[i] = ' ';

    // "Pkt ID: " line
    next_line = &(data_buffer[offset]);
    if((offset = parse_msg(next_line, line)) < 0) return;

    data_buffer[offset] = 0;
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
    else if(!strcmp(type_str, "RREP")) type = RREP_PKT;
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
        ft[i].alive = true;
        //if (i+'A'== id)
            //ft[i].alive = true;
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
            ft[row_num].alive = false;
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

void DVRouter::ft_print(char* print_str) // print the forwarding table
{
    char line_buf[64]; bzero(line_buf, 64);
    sprintf(line_buf, "\n+---Forwarding Table of Router %c---+\n", id);
    strcpy(print_str, line_buf);
    for (int i = 0; i < 6; i++)
    { 
        if(ft[i].cost == INT_MAX)
            sprintf(line_buf, "%c | C=%c, outport=%d, destport=%d, %s\n", 
                    ft[i].dest_id, '-', ft[i].out_port, ft[i].dest_port, ft[i].alive?"alive":"dead");
        else
            sprintf(line_buf, "%c | C=%d, outport=%d, destport=%d, %s\n", 
                ft[i].dest_id, ft[i].cost, ft[i].out_port, ft[i].dest_port, ft[i].alive?"alive":"dead");
        strcat(print_str, line_buf);
    }
    strcat(print_str, "+-------------------------------+\n\n");
}

void DVRouter::dv_print(char* print_str) // print the distance vector table
{
    char line_buf[64]; bzero(line_buf, 64);

    sprintf(line_buf, "+------DV Table of Router %c---+\n", id);
    strcpy(print_str, line_buf);

    strcat(print_str,"    ");
    for (int j = 0; j < 6; j++){
        sprintf(line_buf, "%c | ", 'A'+j);
        strcat(print_str, line_buf);
    }
    strcat(print_str, "\n");

    for (int i = 0; i < 6; i++)
    {
        sprintf(line_buf, "%c | ", 'A'+i);
        strcat(print_str, line_buf);
        for (int j = 0; j < 6; j++)
        {
            if(DV[i][j] == INT_MAX) sprintf(line_buf, "%c | ", '-');
            else sprintf(line_buf, "%d | ", DV[i][j]);
            strcat(print_str, line_buf);
        }
        strcat(print_str, "\n");
    }
    strcat(print_str, "+---------------------------+\n\n");
}

bool DVRouter::update(int dv[6], char neighbor_id)
{
    //copy the old dv
    int old_dv[6][6];
    for(int i = 0; i < 6; i++)
        for(int j = 0; j < 6; j++)
            old_dv[i][j] = DV[i][j];

    int my_row_num = id - 'A'; 
    int Neigh_row_num = neighbor_id - 'A'; //neighbor's row_num
    for (int i = 0; i < 6; i++){
        DV[Neigh_row_num][i] = dv[i];
        if(ft[i].dest_id == neighbor_id && dv[i] == INT_MAX)
            ft[i].alive = false;
    }

    for (int i = 0; i < 6; i++)
    {
        char lastid = ft[i].dest_id;
        if (DV[lastid-'A'][i]!=INT_MAX && ft[i].realcost()!=INT_MAX)
            DV[my_row_num][i] = ft[i].realcost() + DV[lastid-'A'][i];
        else if (!ft[i].alive)
        {
            DV[my_row_num][i] = INT_MAX;
            if(is_neighbor('A'+i)) continue;
        }
        else if(DV[lastid-'A'][i]==INT_MAX)
            DV[my_row_num][i] = INT_MAX;

        for (int j = 0; j < 6; j++)
        {
            if (DV[j][i]==INT_MAX || ft[j].realcost() == INT_MAX || j==my_row_num)
                {continue;}
            if (DV[my_row_num][i] == ft[j].realcost() + DV[j][i]) // if the same total cost, alphabet order!
            {
                if (ft[j].dest_id < ft[i].dest_id)
                {
                    if (j == my_row_num)
                        {continue;}
                    ft[i].dest_id = j+'A';
                    ft[i].dest_port =  port_no(j+'A');
                    ft[i].alive = true;
                }
            }
            else if (DV[my_row_num][i] > ft[j].realcost() + DV[j][i])// need to update DV!
            {
                DV[my_row_num][i] = ft[j].realcost() + DV[j][i];
                ft[i].dest_id = j+'A';
                ft[i].dest_port =  port_no(j+'A');
                ft[i].alive = true;
            }
        }
    }
    return dv_changed(old_dv, DV);
}

bool DVRouter::dv_changed(int old_dv[6][6], int new_dv[6][6])
{
    for(int i = 0; i < 6; i++)
        for(int j = 0; j < 6; j++)
            if(old_dv[i][j] != new_dv[i][j])
                return true;
    return false;
}

void DVRouter::mark_dead_router(char rid)
{
    int max_dv[6]; 
    for(int j=0; j<6; j++)
        max_dv[j] = INT_MAX;
    update(max_dv, rid);
    ft[rid-'A'].alive = false;
    //DV[id-'A'][rid-'A'] = INT_MAX;
}

bool valid_router_id(char id){
    return id == 'A' || id == 'B' || id == 'C'
        || id == 'D' || id == 'E' || id == 'F'
        || id == 'H';
}

int port_no(char id){
    if(valid_router_id(id))
        return 10000 + id - 'A';
    return -1;
}

