/*
 * (c) 2008-2011 Daniel Halperin <dhalperi@cs.washington.edu>
 */
#include "iwl_connector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>



#define MAX_PAYLOAD 2048
#define SLOW_MSG_CNT 1
#define FILE_ORDER_NUM 10
#define FILE_GROUP_NUM 20


int sock_fd = -1;							// the socket
FILE* out = NULL;
FILE* out_buf[FILE_GROUP_NUM][FILE_ORDER_NUM];
char buf_file_name[10] = "buf_data"; 

/* Local variables */
struct sockaddr_nl proc_addr, kern_addr;	// addrs for recv, send, bind
struct cn_msg *cmsg;
char buf[4096];
int ret;
unsigned short l, l2;
int count = 0;
int i,j,k;



void check_usage(int argc, char** argv);

FILE* open_file(char* filename, char* spec);

void caught_signal(int sig);

void exit_program(int code);
void exit_program_err(int code, char* func);


void buf_file_close(int group_num,int order_num)
{
	if (out_buf[group_num][order_num])
	{
		fclose(out_buf[group_num][order_num]);
		out_buf[group_num][order_num] = NULL;
	}
}

void buf_file_open(int group_num,int order_num)
{
	buf_file_name[8] = (char)(group_num + (int)'0');
	buf_file_name[9] = (char)(order_num + (int)'0');
	out_buf[group_num][order_num] = open_file(buf_file_name, "w");
}

void init_socket(char * log_name)
{
	/* Make sure usage is correct */
	//check_usage(argc, argv);

	/* Open and check log file */
	out = open_file(log_name, "w");

	
	for(i = 0; i < FILE_GROUP_NUM; i++)
	{
		for(j = 0;j < FILE_ORDER_NUM; j++){
		out_buf[i][j] = NULL;
		}
	}
	


	// for(file_init_num = 0;file_init_num < file_count; file_init_num++){
	// 	buf_file_name[8] = (char)(file_init_num + (int)'0');
	// 	out_buf[file_init_num] = open_file(buf_file_name, "w");	
	// }

	/* Setup the socket */
	sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (sock_fd == -1)
		exit_program_err(-1, "socket");

	/* Initialize the address structs */
	memset(&proc_addr, 0, sizeof(struct sockaddr_nl));
	proc_addr.nl_family = AF_NETLINK;
	proc_addr.nl_pid = getpid();			// this process' PID
	proc_addr.nl_groups = CN_IDX_IWLAGN;
	memset(&kern_addr, 0, sizeof(struct sockaddr_nl));
	kern_addr.nl_family = AF_NETLINK;
	kern_addr.nl_pid = 0;					// kernel
	kern_addr.nl_groups = CN_IDX_IWLAGN;

	/* Now bind the socket */
	if (bind(sock_fd, (struct sockaddr *)&proc_addr, sizeof(struct sockaddr_nl)) == -1)
		exit_program_err(-1, "bind");

	/* And subscribe to netlink group */
	{
		int on = proc_addr.nl_groups;
		ret = setsockopt(sock_fd, 270, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on));
		if (ret)
			exit_program_err(-1, "setsockopt");
	}

	/* Set up the "caught_signal" function as this program's sig handler */
	signal(SIGINT, caught_signal);
}

int start_read(int group_num,int order_num)
{
	

	/* Poll socket forever waiting for a message */
		/* Receive from socket with infinite timeout */
		ret = recv(sock_fd, buf, sizeof(buf), 0);
		if (ret == -1)
			exit_program_err(-1, "recv");
		/* Pull out the message portion and print some stats */
		cmsg = NLMSG_DATA(buf);
		// if (count % SLOW_MSG_CNT == 0)
		// 	printf("received %d bytes: id: %d val: %d seq: %d clen: %d\n", cmsg->len, cmsg->id.idx, cmsg->id.val, cmsg->seq, cmsg->len);
		/* Log the data to file */
		l = (unsigned short) cmsg->len;
		l2 = htons(l);
		fwrite(&l2, 1, sizeof(unsigned short), out);
		ret = fwrite(cmsg->data, 1, l, out);
		
		fwrite(&l2, 1, sizeof(unsigned short), out_buf[group_num][order_num]);
		ret = fwrite(cmsg->data, 1, l, out_buf[group_num][order_num]);

		// if (count % 100 == 0)
		// 	printf("wrote %d bytes [msgcnt=%u]\n", ret, count);
		++count;
		if (ret != l)
			exit_program_err(1, "fwrite");
	//if (out_buf)
	//{
		//fclose(out_buf);
		//out_buf = NULL;
	// 	}
	

	//exit_program(0);
	return 0;
}

void check_usage(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
		exit_program(1);
	}
}

FILE* open_file(char* filename, char* spec)
{
	FILE* fp = fopen(filename, spec);
	if (!fp)
	{
		perror("fopen");
		exit_program(1);
	}
	return fp;
}

void caught_signal(int sig)
{
	fprintf(stderr, "Caught signal %d\n", sig);
	exit_program(0);
}

void exit_program(int code)
{
	if (out)
	{
		fclose(out);
		out = NULL;
	}
	if (sock_fd != -1)
	{
		close(sock_fd);
		sock_fd = -1;
	}
	exit(code);
}

void exit_program_err(int code, char* func)
{
	perror(func);
	exit_program(code);
}