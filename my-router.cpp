#include "my-router.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <iostream>
#include <fstream> 
#include <climits>
using namespace std;

DVRouter::DVRouter(char rid){
	id = rid; port = port_no(rid);
}

int main()
{


    DVRouter B = DVRouter('F');
    B.ft_init();
    B.ft_print();
    B.dv_init();
    B.dv_print();
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

