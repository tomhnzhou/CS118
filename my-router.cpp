#include "my-router.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

DVRouter::DVRouter(char rid){
	id = rid; port_no = port_no(rid);

}

void DVRouter::ft_init(){

}
