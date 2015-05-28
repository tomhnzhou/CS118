#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
 

typedef struct{
	char dest_id;
	int cost;
	int out_port;
	int dest_port;
} FTEntry;

class DVRouter
{
public:
	DVRouter(char rid); // initializer
	void ft_init();   // initialize forwarding table
    void dv_init();   // initialize distance vector table, need to ft_init first
    void ft_print();  // print the forwarding table
    void dv_print();  // print the distance vector table
	char id;          // my id (A ~ F)
	int port;      // my port number
	int socketfd;     // my socket number
    char neighbor[6]; // who are my neighbors?
	FTEntry ft[6];    // the forwarding table
    int DV[6][6];     // the distance vector table
};

int port_no(char id){
	if(id == 'A' || id == 'B' || id == 'C'
		|| id == 'D' || id == 'E' || id == 'F')
		return 10000 + id - 'A';
	return -1;
}