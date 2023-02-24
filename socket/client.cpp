#include<WINSOCK2.H>
#include<STDIO.H>
#include<iostream>
#include<cstring>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

int main()
{
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA data;
	//存放被WSAStartup函数调用后返回的Windows Sockets数据的数据结构
	if(WSAStartup(sockVersion, &data)!=0)
	{
		return 0;
	}
	printf("请输入：");
	while(1){
		string data;
		cin>>data;

		//创建流式通讯socket
		SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		//地址类型为AD_INET，服务类型为流式(SOCK_STREAM)，协议采用TCP
		if(ClientSocket == INVALID_SOCKET)
		{
			printf("socket创建失败 !\n");
			return 0;
		}
		
		//连接目的IP地址和端口
		sockaddr_in ServerAddr;
		ServerAddr.sin_family = AF_INET;
		ServerAddr.sin_port = htons(7777);
		ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
		//客户端：请求与服务端连接
		//int ret = connect(Client_Sock, …)
		if(connect(ClientSocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
		{  //连接失败 
			printf("连接失败 !\n");
			closesocket(ClientSocket);
			return 0;
		}
		
		//发送数据
		//string data;
		//cin>>data;
		const char * sendData;
		sendData = data.c_str();   
		//string转const char* 
		//请求与服务端连接char buf[1024].
		send(ClientSocket, sendData, strlen(sendData), 0);
		//send()用来将数据由指定的socket传给对方主机
		//int send(int socket, const void * msg, int len, unsigned int flags)
		//socket为已建立好连接的socket，msg指向数据内容，len则为数据长度，参数flags一般设0
		//成功则返回实际传送出去的字符数，失败返回-1，错误原因存于error 


		//等待服务端处理以后返回数据
		//接收数据
		char recData[1024];
		int ret = recv(ClientSocket, recData, 1024, 0);
		if(ret>0){
			recData[ret] = 0x00;
			printf("%s ",recData);
		} 

		//关闭客户端socket
		closesocket(ClientSocket);
	}
	
	
	WSACleanup();
	return 0;
	
}