#include "user_httpserver.h"


static char ssid[33]={0};
static char pwd[65]={0};

static int  http_port=80;
static int  max_client_num=1; //服务器最多能同时连接的客户端数量
static int  cur_client_num=0; //服务器当前连接客户端数量


static TaskHandle_t http_start_task_handle=NULL; //accept任务handle
static int serverfd = -1; //accept任务fd


static char*        *p_request_url = NULL;  //每个客户端url buf的总指针
static TaskHandle_t *p_process_handle=NULL; //每个process task handle总指针
static int          *p_connfd = NULL;       //每个客户端 fd 总指针


void unimplemented(int client,char send_flag) 
{
    char buff[] =
        "HTTP/1.0 501 Method Not Implemented\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "Method Not Implemented";
	if(send_flag)
    	send(client, buff, sizeof(buff), 0);
}

void not_found(int client,char send_flag)
{
    char buff[] =
        "HTTP/1.0 404 NOT FOUND\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "The resource specified is unavailable.\r\n";
	if(send_flag)
    	send(client, buff, strlen(buff), 0);
}

void url_decode(const char *src, char *dest) 
{
    const char *p = src;
    char code[3] = {0};
    while (*p && *p != '?') 
	{
        if(*p == '%') 
		{
            memcpy(code, ++p, 2);
            *dest++ = (char)strtoul(code, NULL, 16);
            p += 2;
        } 
		else 
		{
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}

int get_param(char *arg,char *ssid,char* pwd)
{
//wifiname=xindalu&wifipasswd=123456789
	if(arg == NULL || ssid== NULL || pwd == NULL)
		return -1;
	
	char *PA=NULL,*PB=NULL;
	PA=strstr(arg, "wifiname=");
	if(PA==NULL) return -1;
	PA+=strlen("wifiname=");
	PB=strstr(PA, "&");
	if(PB==NULL) return -1;
	memcpy(ssid, PA, PB-PA);
	ssid[PB-PA] = '\0';
	
	PA=strstr(arg, "wifipasswd=");
	if(PA==NULL)return -1;
	PA+=strlen("wifipasswd=");
	PB=arg+strlen(arg);
	if(PB==NULL) return -1;
	memcpy(pwd, PA, PB-PA);
	pwd[PB-PA] = '\0';	

	return 0;
}

void socket_response(int socket,const char* buf,int buf_len)
{
	if(socket == -1 || buf == NULL || buf_len == 0) return;
	
	int need_send = 0;int cnt=0;int send_ret=0;
	while(cnt < buf_len)
	{
		if(buf_len - cnt > 1024)
			need_send = 1024;
		else
			need_send = buf_len - cnt;
		
		if((send_ret = send(socket, buf+cnt, need_send, 0)) < 0)
		  break;
		cnt+=send_ret;
	}
}


//options scanap linkweb 
void do_options(int sockfd, const char *uri,char send_flag) 
{
	char filename[MAX_URI_LEN] = {0};
	int do_options_flag=0;
    const char *cur = uri + 1;
    size_t len = strlen(cur);
    if (len == 0) 
	{
        strcpy(filename, "index.html");
		do_options_flag = 0;
    } 
	else 
	{
        url_decode(cur, filename);
    }
	printf("\nfilename:%s\n", filename);
	if(strcmp(filename,"linkweb")==0)
	{
		do_options_flag = 1;
	}
	else if(strcmp(filename,"scanap")==0)
	{
		do_options_flag = 1;
	}
	else
	{
		do_options_flag = -1;
	}
	if(do_options_flag == -1 || do_options_flag == 0)
	{
		not_found(sockfd,send_flag);
        return;
	}
	char header[256]={0};
	//char header_tmp[64]={0};
	memset(header,0,sizeof(header));
	strcat(header,"HTTP/1.1 200 Ok\r\n");
	strcat(header,"Access-Control-Allow-Origin: * \r\n");
	strcat(header,"Access-Control-Allow-headers: x-requested-with,content-type \r\n");
	strcat(header,"Content-Type: text/plain\r\n");
	strcat(header,"Connection: close\r\n");	
	strcat(header,"\r\n");
	if(send_flag)
	{
		socket_response(sockfd,header,strlen(header));
	}
}


//get scanap
void do_get(int sockfd, const char *uri,char send_flag) 
{
    char filename[MAX_URI_LEN] = {0};
	int do_get_flag=0;
    const char *cur = uri + 1; // '/'
    size_t len = strlen(cur); 
    if (len == 0) 
	{
        strcpy(filename, "index.html");
		do_get_flag = 0;
    } 
	else 
	{
        url_decode(cur, filename);
    }
    printf("\nfilename:%s\n", filename);

	if(strcmp(filename,"scanap")==0)
	{
		do_get_flag = 1;
	}
	else
	{
		do_get_flag = -1;
	}
	
	if(do_get_flag == -1 || do_get_flag == 0)
	{
		not_found(sockfd,send_flag);
        return;
	}
	unsigned char header[256]={0};
	unsigned char body[256] = {0};
	
	uint32_t *body_len = 0;
	unsigned char * p_body=NULL;
	unsigned char header_tmp[64]={0};
	
	memset(header,0,sizeof(header));
	strcat(header,"HTTP/1.1 200 OK\r\n");
	strcat(header,"Access-Control-Allow-Origin: * \r\n");
	strcat(header,"Access-Control-Allow-headers: x-requested-with,content-type \r\n");
	strcat(header,"Connection: close\r\n"); 
	
	if(do_get_flag == 1) //1-scanap
	{
		strcat(header,"Content-Type: text/plain\r\n");
		//{"errcode":0,"errmsg":"ssid1,ssid2,ssid3,ssid4"}
		memset(body,0,sizeof(body));
		sprintf(body,"{\"errcode\":0,\"errmsg\":\"ssid1,ssid2,ssid3,ssid4\"}");
		*body_len = strlen(body);
		p_body = body;
	}
get_exit:
	memset(header_tmp,0,sizeof(header_tmp));
	sprintf(header_tmp,"Content-Length: %d\r\n",*body_len);	
	strcat(header,header_tmp);
	strcat(header,"\r\n");
	if(send_flag)
	{
		socket_response(sockfd,header,strlen(header));
		socket_response(sockfd,p_body,*body_len);
	}
}


//post linkweb 
void do_post(int sockfd, const char *uri,const char* client_body_data,char send_flag) 
{
	char filename[MAX_URI_LEN] = {0};
	int do_post_flag=0;
    const char *cur = uri + 1;
    size_t len = strlen(cur);
    if (len == 0) 
	{
        strcpy(filename, "index.html");
		do_post_flag = 0;
    } 
	else 
	{
        url_decode(cur, filename);
    }
	printf("\nfilename:%s\n", filename);
	
	if(strcmp(filename,"linkweb")==0)
	{
		if(get_param(client_body_data,ssid,pwd) == 0)
		{
			do_post_flag = 1;
			printf("\nssid:%s\n",ssid);
			printf("\npassword:%s\n",pwd);
		}
	}
	else
	{
		do_post_flag = -1;
	}
	if(do_post_flag == -1 || do_post_flag == 0)
	{
		not_found(sockfd,send_flag);
        return;
	}

	char header[256]={0};
	char body[256] = {0};
	char header_tmp[64]={0};
	
	uint32_t *body_len = 0;
	unsigned char * p_body=NULL;
	
	memset(header,0,sizeof(header));
	strcat(header,"HTTP/1.1 200 Ok\r\n");
	strcat(header,"Access-Control-Allow-Origin: * \r\n");
	strcat(header,"Access-Control-Allow-headers: x-requested-with,content-type \r\n");

	strcat(header,"Connection: close\r\n");	

	if(do_post_flag == 1) //1-linkweb
	{
		strcat(header,"Content-Type: text/plain\r\n");
		//{"errcode":0,"errmsg":"OK"}
		memset(body,0,sizeof(body));
		sprintf(body,"{\"errcode\":0,\"errmsg\":\"OK\"}");
		p_body = body;
		*body_len = strlen(body);
		goto post_exit;
	}
post_exit:
	memset(header_tmp,0,sizeof(header_tmp));
	sprintf(header_tmp,"Content-Length: %d\r\n",*body_len);	
	strcat(header,header_tmp);
	strcat(header,"\r\n");
	if(send_flag)
	{
		socket_response(sockfd,header,strlen(header));
		socket_response(sockfd,p_body,*body_len);
	}
}


void server_deal_with_method(http_parser *parser,const char* url,const char* clibody,char send_flag)
{
	printf("httpMethod:");
	if( parser->method == 1)
	{
		printf("GET");   
		//do_get(*(int*)(parser->data),url,send_flag);
		do_get((*(http_parse_data_t*)(parser->data)).sockfd,url,send_flag);
	}
	else if( parser->method == 3)
	{
		printf("POST");
		//do_post(*(int*)(parser->data),url,clibody,send_flag);
		do_post((*(http_parse_data_t*)(parser->data)).sockfd,url,clibody,send_flag);
	}
	else if( parser->method == 6)
	{
		printf("OPTIONS");
		//do_options(*(int*)(parser->data),url,send_flag);
		do_options((*(http_parse_data_t*)(parser->data)).sockfd,url,send_flag);
	}
	else
	{ 
		printf("NOT SUPPORT");
		//unimplemented(*(int*)(parser->data),send_flag);
		unimplemented((*(http_parse_data_t*)(parser->data)).sockfd,send_flag);
	}
	printf("\n");
}


void display_data(const char* buf, size_t length)
{
	char tempData[256] = {0};
	int  cnt = 0;
	int  length_tmp = length;
	int  operate_len = 0;
	while(cnt < length_tmp)
	{
		if(length_tmp - cnt >= 256)
			operate_len = 256;
		else
			operate_len = length_tmp - cnt;
		
		memset(tempData,0,sizeof(tempData));
		memcpy(tempData,buf+cnt,operate_len);
		printf("%s",tempData);
		cnt += operate_len;
	}
	printf("\r\n");
}

int32 httpMessageBeginCb(http_parser *parser)
{
	printf("%s(%d):\r\n", __func__,__LINE__);
	(*(http_parse_data_t*)(parser->data)).add_body_length = 0;
  	return 0;
}

int httpUrlCb(http_parser *parser, const char* buf, size_t length)
{
	printf("%s(%d)-(%d):", __func__,__LINE__,length);
	display_data(buf,length);
	memset(p_request_url[(*(http_parse_data_t*)(parser->data)).url_index],0,MAX_RESPUST_LEN*sizeof(char));
	if(MAX_RESPUST_LEN >= length)
	{
		memcpy(p_request_url[(*(http_parse_data_t*)(parser->data)).url_index],buf,length);
	}
	else
	{
		memcpy(p_request_url[(*(http_parse_data_t*)(parser->data)).url_index],buf,MAX_RESPUST_LEN);
		printf("request_url too long\n");
	}

	return 0;
	
}

int32 httpStatusCb(http_parser *parser, const char *buf, u32 length)
{	
	printf("%s(%d)-(%d):", __func__,__LINE__,length);
	display_data(buf,length);
  	return 0;
}

int32 httpHeaderFieldCb(http_parser *parser, const char *buf, u32 length)
{  
	printf("%s(%d)-(%d):", __func__,__LINE__,length);
	display_data(buf,length);

	
   if(memcmp(buf,"content-length",strlen("content-length")) == 0 ||\
   	  memcmp(buf,"Content-Length",strlen("Content-Length")) == 0)
   	{
		if((*(http_parse_data_t*)(parser->data)).content_length_flag == 0)
			(*(http_parse_data_t*)(parser->data)).content_length_flag = 1;
	}
	if(memcmp(buf,"Content-Range",strlen("Content-Range")) == 0)
	{
		//printf("get Content-Range\n");
	}
  	return 0;
}

int32 httpHeaderValueCb(http_parser *parser, const char *buf, u32 length)
{ 
	printf("%s(%d)-(%d):", __func__,__LINE__,length);
	display_data(buf,length);
	if((*(http_parse_data_t*)(parser->data)).content_length_flag == 1)
	{
		(*(http_parse_data_t*)(parser->data)).content_length_flag = 2;
		(*(http_parse_data_t*)(parser->data)).content_length_val = strtol(buf,NULL,10);
	}
  	return 0;
}

int32 httpHeaderCompleteCb(http_parser *parser)
{
	printf("%s(%d): \r\n", __func__,__LINE__);
	(*(http_parse_data_t*)(parser->data)).header_complete_flag = 1;
  	return 0;
}

int32 httpBodyCb(http_parser *parser, const char *buf, u32 length)
{

	printf("%s(%d)-(%u):", __func__,__LINE__,length);
	display_data(buf,length);
	(*(http_parse_data_t*)(parser->data)).client_body_flag = 1;

	(*(http_parse_data_t*)(parser->data)).add_body_length += length;
	if((*(http_parse_data_t*)(parser->data)).add_body_length == (*(http_parse_data_t*)(parser->data)).content_length_val)
	{
		(*(http_parse_data_t*)(parser->data)).add_body_length = 0;
		server_deal_with_method(parser,p_request_url[(*(http_parse_data_t*)(parser->data)).url_index],buf,1);
	}
	else
	{
		server_deal_with_method(parser,p_request_url[(*(http_parse_data_t*)(parser->data)).url_index],buf,0);
	}
	return 0;
}

int32 httpMessageCompleteCb(http_parser *parser)
{
	printf("%s(%d): \r\n", __func__,__LINE__);
  	return 0;
}


static void process(void* psockfd) 
{
	http_parse_data_t  http_parse_data;
	memset(&http_parse_data,0,sizeof(http_parse_data));
	http_parse_data.sockfd = http_parse_data.url_index = -1;

    int sockfd = *(int*)psockfd;        //add sockfd
	http_parse_data.sockfd = sockfd;
	int j=0,j_handle_bak=0,j_fd_bak=0,j_url_bak=0;;
	
    for(j=0;j<max_client_num;j++)
    {
		if(p_process_handle[j] == NULL)
		{
			p_process_handle[j] = xTaskGetCurrentTaskHandle();
			j_handle_bak = j;
			break;
		}
	}
	printf("process_handle cre: %x\n",p_process_handle[j_handle_bak]);
    for(j=0;j<max_client_num;j++)
    {
		if(p_connfd[j] == -1)
		{
			p_connfd[j] = sockfd;
			j_fd_bak = j;
			break;
		}
	}	
	printf("process_fd cre: %d\n",p_connfd[j_fd_bak]);

    for(j=0;j<max_client_num;j++)
    {
		if(p_request_url[j] == NULL)
		{
			p_request_url[j] = malloc(MAX_RESPUST_LEN*sizeof(char));
			if(p_request_url[j] == NULL)
				printf("malloc(MAX_RESPUST_LEN) fail!!!\n");
			memset(p_request_url[j],0,MAX_RESPUST_LEN*sizeof(char));
			j_url_bak = j;
			break;
		}
	}	
	if(p_request_url[j_url_bak] != NULL)
	{ 
		http_parse_data.url_index = j_url_bak;   // add  url_index
	}
	else
	{
		p_request_url[j_url_bak] = NULL;
		http_parse_data.url_index = -1;
		printf("request_url NULL!!!\n");
	}
	
	printf("process_url cre: %x\n",p_request_url[j_url_bak]);



	char http_receive_data_buf[HTTP_REVEIVE_DATA_LEN] = {0};
	http_parser_settings httpParserSettings;
	http_parser httpParser;

	//设置http_praser的回调函数
	http_parser_settings_init(&httpParserSettings);
	
	httpParserSettings.on_message_begin = httpMessageBeginCb;
	httpParserSettings.on_url = httpUrlCb;
	httpParserSettings.on_status = httpStatusCb;
	httpParserSettings.on_header_field = httpHeaderFieldCb;
	httpParserSettings.on_header_value = httpHeaderValueCb;
	httpParserSettings.on_headers_complete = httpHeaderCompleteCb;
	httpParserSettings.on_body = httpBodyCb;
	httpParserSettings.on_message_complete = httpMessageCompleteCb;
	//httpParser.data = &sockfd;
	httpParser.data = &http_parse_data;
	http_parser_init(&httpParser, HTTP_BOTH);
	
	int need_read   = 0;
	int body_need_read   = 0;
	
	//content_length_flag = 0; 
	//content_length_val  = 0;
	
	//header_complete_flag = 0;//头未读完
	//client_body_flag = 0;//是否有实体
	need_read = HTTP_REVEIVE_DATA_LEN;
retry_loop:
	memset(&http_receive_data_buf[0],0,HTTP_REVEIVE_DATA_LEN);
   int recvlen = recv(sockfd, &http_receive_data_buf[0], need_read, 0); 
   if(recvlen > 0)
   {
	   http_parser_execute(&httpParser, &httpParserSettings, (const char *)&http_receive_data_buf[0], recvlen);
	   if(http_parse_data.content_length_flag==3)
	   {
	   		body_need_read -= recvlen;
	   		if(body_need_read > HTTP_REVEIVE_DATA_LEN)
				need_read = HTTP_REVEIVE_DATA_LEN;
			else
				need_read = body_need_read;
			if(need_read == 0)
				goto recv_done;
			else
				goto retry_loop;
	   }
	   
	   if( http_parse_data.header_complete_flag == 0)
	   {
	   		goto retry_loop;
	   }
	   else  if(http_parse_data.content_length_flag == 2) //头读完且有content-length
	   {
			body_need_read = http_parse_data.content_length_val;
			http_parse_data.content_length_flag = 3;
			if(body_need_read !=0)
			{
				if(body_need_read > HTTP_REVEIVE_DATA_LEN)
					need_read = HTTP_REVEIVE_DATA_LEN;
				else
					need_read = body_need_read;
				goto retry_loop;
			}
			else
				goto recv_done;
	   }
   }
   else if(recvlen == 0)
   { 
	   printf("client close\n");
   }
   else if(recvlen < 0)
   {   
	   printf("recv err\n");
	   goto FINAL;
   }
recv_done:         
	if(http_parse_data.client_body_flag == 0)
	{
		server_deal_with_method(&httpParser,p_request_url[j_url_bak],NULL,1);
	}
FINAL:
	printf("process_handle del: %x\n",p_process_handle[j_handle_bak]);
	printf("process_fd del: %d\n",p_connfd[j_fd_bak]);
	printf("process_url del: %x\n",p_request_url[j_url_bak]);

    close(sockfd);
	p_connfd[j_fd_bak] = -1;

	p_process_handle[j_handle_bak] = NULL;
	cur_client_num--;

	if(p_request_url[j_url_bak] != NULL);
	{
		free(p_request_url[j_url_bak]);
		p_request_url[j_url_bak] = NULL;
		http_parse_data.url_index = -1;
	}

	vTaskDelete(NULL);
    return 0;
}



static void http_start_task(void* arg) 
{
	int ii=0;
		
    int connfd = -1;
    struct sockaddr_in client;
    socklen_t clientlen = sizeof(client);
	
	serverfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverfd == -1)
    {
        printf("create socket fail\n");
		goto http_exit;
    }
	printf("serverfd : %d\n",serverfd);
	int on = 1;
	setsockopt(serverfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(http_port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverfd,(struct sockaddr *)&server, sizeof(server)) == -1)
    {
        printf("bind fail\n");
		goto http_exit;
    }
    if (listen(serverfd, 10) == -1)
    {
        printf("listen fail\n");
		goto http_exit;
    }
	
    if(serverfd == -1)
		goto http_exit;
	
    printf("Server started, listen port %d\n", http_port);

	
    while (1) 
	{	
		if(cur_client_num < max_client_num)
		{
			 connfd = accept(serverfd, (struct sockaddr *)&client, &clientlen); 
			 //printf("connfd[%d]:%d\n",cur_client_num,connfd);
			 unsigned char *ip = (unsigned char*)&client.sin_addr.s_addr;
			 unsigned short port = client.sin_port;
			 printf("client info: %u.%u.%u.%u:%5u \n", ip[0], ip[1], ip[2], ip[3], port);
			 
			if( xTaskCreate(process, "process", 1024, &connfd, OS_TASK_PRIO2, NULL) == 1)
			{
				cur_client_num++;
			}
			else 
			{
				printf("create process fail\n");
				goto http_exit;
			}

		}
		else
		{
			OS_MsDelay(100);
		}
    }
http_exit:
	printf("http_exit\n");
	http_task_pre();
    return 0;
}



void http_task_pre(void)
{
	int ii=0;
	if(serverfd != -1) close(serverfd); serverfd = -1;

	if(p_process_handle != NULL)
	{
		for(ii=0;ii<max_client_num;ii++)
		{
			if(p_process_handle[ii] != NULL)
			{
				printf("handle pre:%x\n",p_process_handle[ii]);
				vTaskDelete(p_process_handle[ii]);
				p_process_handle[ii] = NULL;
			}
		}
		free(p_process_handle);
		p_process_handle = NULL;
	}
	if(p_connfd != NULL)
	{
		for(ii=0;ii<max_client_num;ii++)
		{
			if(p_connfd[ii] != -1)
			{
				printf("fd pre:%x\n",p_connfd[ii]);
				close(p_connfd[ii]);
				p_connfd[ii] = -1;
			}
		}
		free(p_connfd);
		p_connfd = NULL;
	}

	if(p_request_url != NULL)
	{
		for(ii=0;ii<max_client_num;ii++)
		{
			if(p_request_url[ii] != NULL)
			{
				printf("url pre:%x\n",p_request_url[ii]);
				free(p_request_url[ii]);
				p_request_url[ii] = NULL;
			}
		}
		free(p_request_url);
		p_request_url = NULL;
	}

	if(http_start_task_handle != NULL)
	{
		printf("start_task:%x\n",http_start_task_handle);
		OS_TaskDelete(http_start_task_handle);
		http_start_task_handle = NULL;
	}
	cur_client_num = 0;
}


void creat_http_task(int port,int max_client)
{	
	max_client_num = max_client;
	if(max_client <=0) return;
	
	http_task_pre();
	http_port = port;	

	p_process_handle=malloc(max_client_num * sizeof(TaskHandle_t)); //存放每个客户端process 任务handle
	if(p_process_handle == NULL) return; 
	memset(p_process_handle,0,max_client_num * sizeof(TaskHandle_t));

    p_connfd = malloc(max_client_num * sizeof(int)); //存放每个客户端fd
	if(p_connfd == NULL)  return;
	memset(p_connfd,-1,max_client_num * sizeof(int));


	p_request_url = malloc(max_client_num*sizeof(char*));//存放每个客户端请求的url的指针   一个url 256字节
	if(p_request_url == NULL) return;
	memset(p_request_url,0,max_client_num*sizeof(char*));

	if( xTaskCreate(http_start_task, "http_start_task", 512, NULL, OS_TASK_PRIO2, &http_start_task_handle) == 1)
	{
		printf("http server setup ok handle: %x\n",http_start_task_handle);
	}
	else
	{
		printf("http server setup fail\n");
	}
}

void delete_http_task(void)
{
	http_task_pre();
	printf("http server del\n");
}


/*
测试http
client:
option /scanap
option /linkweb
get    /scanap  
post   /linkweb
*/

