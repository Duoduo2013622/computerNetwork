#include <stdio.h>  
#include <winsock2.h>  
#include <time.h> 

#pragma comment(lib,"ws2_32.lib")  
  
int main(int argc, char* argv[])  
{  
    //初始化WSA  ,加载Winsock库
    WORD sockVersion = MAKEWORD(2,2);  //声明使用socket2.2版本
    WSADATA wsaData;   //存放被WSAStartup函数调用后返回的Windows Sockets数据
    //初始化socket资源
    if(WSAStartup(sockVersion, &wsaData)!=0)  
    {  
        return 0;  //初始化失败
    }  
  
    //创建流式监听套接字  

    //地址类型为AD_INET，服务类型为流式(SOCK_STREAM)，协议采用TCP 
    SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  

    if(ServerSocket == INVALID_SOCKET)  
    {  
        printf("socket创建失败 !\n");  
        return 0;  
    }  
  
    //绑定IP和端口  
    sockaddr_in listenAddr;  
    listenAddr.sin_family = AF_INET; //地址类型为AD_INET，即IP格式
    listenAddr.sin_port = htons(7777);  //绑定本地监听端口：7777
    listenAddr.sin_addr.S_un.S_addr = INADDR_ANY;   
    if(bind(ServerSocket, (LPSOCKADDR)&listenAddr, sizeof(listenAddr)) == SOCKET_ERROR)  
    {  
        printf("端口绑定失败 !\n");  
        closesocket(ServerSocket);  
	        return 0;  
    }  
  
    //绑定成功就开始监听  
    if(listen(ServerSocket, 5) == SOCKET_ERROR)  
    {  
        printf("监听失败 !\n");  
        return 0;  
    }  
  
      
    
    SOCKET ClientSocket;  
    sockaddr_in acceptAddr;  //远程连接的客户端属性
    int ClientAddrlen = sizeof(acceptAddr);  
    char revData[1024];  

    //循环等待客户端接入
    while (true)  
    {  
        printf("等待连接...\n"); 

        //SOCKET  Command_Sock = accept(Listen_Sock,…) 
        ClientSocket = accept(ServerSocket, (SOCKADDR *)&acceptAddr, &ClientAddrlen);  
        if(ClientSocket == INVALID_SOCKET)  
        {  
            printf("服务器接收请求失败 !\n");  
            //重新等待连接
            continue;  
        } 

        printf("成功接受到一个连接\n");


        



        //接收数据  
        //等待客户接入 revData[1024].
        //接收数据：recv(Command_Sock, buf, …)

        //memset(revData, 0, sizeof(revData));
        int retlen = recv(ClientSocket, revData, 1024, 0);         
        if(retlen > 0)  
        {  
            revData[retlen] = 0x00;  
            time_t nowtime; 
            struct tm *sysTime; 
            nowtime = time(NULL); //获取日历时间
            sysTime=localtime(&nowtime);//转换为系统的日期
            printf("%d-%d-%d  %d:%d:%d：%s\n", 1900+sysTime->tm_year,sysTime->tm_mon+1,sysTime->tm_mday,sysTime->tm_hour,sysTime->tm_min,sysTime->tm_sec,revData);  
        }  

        //发送数据  
        //发送数据：send(Command_Sock, buf, …)
        const char * sendData = "请输入：";  
        send(ClientSocket, sendData, strlen(sendData), 0);  
        //关闭客户端socket
        closesocket(ClientSocket);  
    }  
      
    //关闭服务端socket
    closesocket(ServerSocket);  

    //释放Winsock库，释放socket库
    WSACleanup();  
    return 0;  
} 