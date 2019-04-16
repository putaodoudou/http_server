#ifndef _USER_HTTPSERVER_H_
#define _USER_HTTPSERVER_H_

#include "sockets.h"
#include "http_parser.h"

#define MAX_REQUEST_LEN 1024
#define MAX_METHOD_LEN  32
#define MAX_URI_LEN     256

#define MAX_RESPUST_LEN     256


#define HTTP_REVEIVE_DATA_LEN  1024
#define HTTP_SEND_DATA_LEN     1024

typedef struct{
	int  sockfd;    				//客户端fd
	int  url_index; 				//客户端url index
	int	 content_length_val;		//body 长度
	u32  add_body_length; 		    //累计接受到的实体字节数
	char client_body_flag; 		    //客户端发来数据是否有body 0 无 1 有
	char header_complete_flag; 	    //头是否读完标志 0未读完 1读完
	char content_length_flag;		//content_length标志 0为获得content_length标志 1获得 2取得body长度 3取得需要再次读取值
}http_parse_data_t;   //每个客户端内部维护参数

void creat_http_task(int port,int max_client);

void delete_http_task(void);


#endif /* _USER_HTTPSERVER_H_ */

