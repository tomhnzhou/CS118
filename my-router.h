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
	DVRouter(char rid);
	void ft_init();
	char id;
	int port_no;
	int socketfd;
	FTEntry ft[6];

};

int port_no(char id){
	if(id == 'A' || id == 'B' || == 'C'
		|| id == 'D' || id == 'E' || id == 'F')
		return 10000 + id - 'A';
	return -1;
}