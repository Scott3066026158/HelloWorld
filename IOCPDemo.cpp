#include "stdafx.h"
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <iostream>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"kernel32.lib")
int server();
DWORD WINAPI WorkerThread (LPVOID WorkThreadContext);
/* 
typedef struct _overlapped {
ulong_ptr internal; //操作系统保留，指出一个和系统相关的状态
ulong_ptr internalhigh; //指出发送或接收的数据长度
union {
struct {
dword offset; //文件传送的字节偏移量的低位字
dword offsethigh; //文件传送的字节偏移量的高位字
};
pvoid pointer; //指针，指向文件传送位置
};
handle hevent; //指定一个i/o操作完成后触发的事件
} OVERLAPPED, *LPOVERLAPPED;
*/

enum IO_OPERATION{IO_READ,IO_WRITE};


//typedef struct _WSABUF {
//    ULONG len;								数组长度
//    _Field_size_bytes_(len) CHAR FAR *buf;    char*
//} WSABUF, FAR * LPWSABUF;

struct IO_DATA{
    OVERLAPPED                  Overlapped;  //
    WSABUF                      wsabuf;
    IO_OPERATION                opCode;     //读写标志
    SOCKET                      client;
};
HANDLE h_IOCP;
char buffer[1024];
int main()
{
	int result = server();
	if(result == -1)
	{
		printf("服务启动失败！\n");
	}
	system("pause");
	return 0;
}
int server()
{
	//接收版本信息
    WSADATA socketVersion;
	//获取版本信息
    int result = WSAStartup(MAKEWORD(2,2), &socketVersion);
	if(result != 0)
	{
		printf("socket库绑定失效！\n");
		return -1;
	}
	//af：[in]一个地址族规范。目前仅支持AF_INET格式。
	//type：新套接口的类型描述。
	//protocol：套接口使用的特定协议，如果调用者不愿指定协议则定为0。
	//lpProtocolInfo：一个指向PROTOCOL_INFO结构的指针，该结构定义所创建套接口的特性。如果本参数非零，则前三个参数（af, type, protocol）被忽略。
	//g：保留给未来使用的套接字组。套接口组的标识符。
	//iFlags：套接口属性描述。
	//return：IOsocket描述符
    UINT m_socket = WSASocket(AF_INET,SOCK_STREAM, IPPROTO_TCP, 0,0,WSA_FLAG_OVERLAPPED);
	if(m_socket == -1)
	{
		printf("Socket创建失败！\n");
		return -1;
	}
	//创建监听的地址信息
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(9990);
    server.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//绑定
    result = bind(m_socket ,(sockaddr*)&server,sizeof(server));
	if(result == -1)
	{
		printf("Socket绑定失败！\n");
		return -1;
	}
	//开始监听
    result = listen(m_socket, 5);
	if(result == -1)
	{
		printf("Socket监听失败！\n");
		return -1;
	}
	//创建存储系统信息的结构体
    SYSTEM_INFO sysInfo;
	//获取系统信息
    GetSystemInfo(&sysInfo);
	//并发数 = 处理器数 * 2(为何不和处理器的数量一样：为了更好的利用系统性能)
    int ConcurrentCount = sysInfo.dwNumberOfProcessors * 2;
	//创建IOCP,返回一个句柄
    h_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,ConcurrentCount);
	//遍历启动指定的WorkerThread
    for(int i = 0;i < ConcurrentCount; ++i){
        DWORD   dwThreadId;
        HANDLE  hThread = CreateThread(NULL, 0, WorkerThread, 0, 0, &dwThreadId);
        CloseHandle(hThread);
    }
	//循环接收客户端的连接
    while(1)
    {
		printf("等待连接...\n");
		//接收连接
        SOCKET client = accept(m_socket, NULL, NULL);
        printf("已接收客户端的连接");
		//没接收一个Socket就将其和IOCP绑定
        if (CreateIoCompletionPort((HANDLE)client, h_IOCP, 0, 0) == NULL)
		{
			printf("将IOCP与客户端连接的Socket绑定失败！失败代码：%u\n",GetLastError());
            closesocket(client);
        }
        else 
		{
			//创建一个存储发送或接收数据信息的结构体
            IO_DATA* data = new IO_DATA;
			//初始化一个char数组
            memset(buffer, 0 ,1024);
			//初始化重叠结构体(其实重叠我觉得叫异步更易理解)
            memset(&data->Overlapped, 0 , sizeof(data->Overlapped));
			//初始设定是读取操作
            data->opCode = IO_READ;
			//初始化WSABUF
            data->wsabuf.buf  = buffer;
            data->wsabuf.len  = sizeof(buffer);
			//将接收的Socket描述符交给data
            data->client = client;
			//初始化接收数据的大小
            DWORD nBytes = 1024;
			//初始化接收数据的大小
			DWORD dwFlags = 0;
			/*
			s：一个标识已连接套接口的描述字。
			lpBuffers：一个指向WSABUF结构数组的指针。每一个WSABUF结构包含一个缓冲区的指针和缓冲区的长度。
			dwBufferCount：lpBuffers数组中WSABUF结构的数目。
			lpNumberOfBytesRecvd：如果接收操作立即结束，一个指向本调用所接收的字节数的指针。
			lpFlags：一个指向标志位的指针。
			lpOverlapped：一个指向WSAOVERLAPPED结构的指针（对于非重叠套接口则忽略）。
			lpCompletionRoutine：一个指向接收操作结束后调用的例程的指针（对于非重叠套接口则忽略）。
			*/
            int nRet = WSARecv(client, &data->wsabuf, 1, &nBytes, &dwFlags, &data->Overlapped, NULL);
            if(nRet == SOCKET_ERROR  && ERROR_IO_PENDING != WSAGetLastError()){
				printf("向操作系统投递接收操作失败！原因代码:%d\n", WSAGetLastError());
                closesocket(client);
                delete data;
            }
			//输出接收到的信息
			printf("%s\n", data->wsabuf.buf);
        }
    }
    closesocket(m_socket);
    WSACleanup();
}

DWORD WINAPI WorkerThread (LPVOID WorkThreadContext) {
	
	//接收从完成队列里获取的数据地址
    LPOVERLAPPED lpOverlapped = NULL;
    IO_DATA *lpIOContext = NULL; 	
	//数据长度
    DWORD dwIoSize = 0;
	//接收创建线程时定义的key
    void* lpCompletionKey = NULL;
	//定义操作数据的大小
	DWORD nBytes = 0;
	//标识位
	DWORD dwFlags = 0;
	//投递后操作数据的大小
	int nRet = 0;
    while(1)
	{
		//循环询问完成队列中有没有完成的操作
        GetQueuedCompletionStatus(h_IOCP, &dwIoSize,(LPDWORD)&lpCompletionKey,&lpOverlapped, INFINITE);
        lpIOContext = (IO_DATA*)lpOverlapped;
		//若发送/接收完成动作的数据大小为0，则关闭客户端的连接
        if(dwIoSize == 0)
        {
			printf("从完成队列中获取元素失败！原因代码:%d\n", WSAGetLastError());
			printf("此客户端连接已断开:%u\n", lpIOContext->client);
            closesocket(lpIOContext->client);
            delete lpIOContext;
            continue;
        }
		//若完成队列读取到的opcode是read的话
        if(lpIOContext->opCode == IO_READ)
        {
			//打印已经完成接收的数据
			printf("服务器收到的信息：%s\n", lpIOContext->wsabuf.buf);
			//清空缓存
            ZeroMemory(&lpIOContext->Overlapped, sizeof(lpIOContext->Overlapped));
			//重新写入将要发送的数据
            lpIOContext->wsabuf.buf = "我是服务器";
			//设置发送数据大小
            lpIOContext->wsabuf.len = strlen(buffer)+1;
            nBytes = lpIOContext->wsabuf.len;
			//改变标识
            lpIOContext->opCode = IO_WRITE;
			//标识设0
            dwFlags = 0;
			//向驱动或系统投递一个发送请求
            nRet = WSASend(lpIOContext->client, &lpIOContext->wsabuf, 1, &nBytes, dwFlags, &(lpIOContext->Overlapped), NULL);
			//若是请求投递失败，则关闭此Socket连接
            if(nRet == SOCKET_ERROR && ERROR_IO_PENDING != WSAGetLastError() ) 
			{
				printf("向操作系统投递发送操作失败！原因代码:%d\n", WSAGetLastError());
				printf("此客户端连接已断开:%u\n", lpIOContext->client);
                closesocket(lpIOContext->client);
                delete lpIOContext;
                continue;
            }
			//清空缓存
            memset(buffer, NULL, sizeof(buffer));
        }
		//若完成队列读取到的opcode是read的话
        else if(lpIOContext->opCode == IO_WRITE)
        {
			//打印已经完成接收的数据
			printf("服务器发送的信息：%s\n", lpIOContext->wsabuf.buf);
			//改变标识
            lpIOContext->opCode = IO_READ; 
			//设置接收数据大小
            nBytes = 1024;
			//标识设0
            dwFlags = 0;
			//设置接收数据的数组
            lpIOContext->wsabuf.buf = buffer;
			//设置接收数据的数组的大小
            lpIOContext->wsabuf.len = nBytes;
			//清空缓存
            ZeroMemory(&lpIOContext->Overlapped, sizeof(lpIOContext->Overlapped));
			//向驱动或系统投递一个发送请求
            nRet = WSARecv(lpIOContext->client,&lpIOContext->wsabuf, 1, &nBytes, &dwFlags, &lpIOContext->Overlapped, NULL);
            if(nRet == SOCKET_ERROR && ERROR_IO_PENDING != WSAGetLastError()) 
			{
                printf("向操作系统投递接收操作失败！原因代码:%d\n", WSAGetLastError());
				printf("此客户端连接已断开:%u\n", lpIOContext->client);
                closesocket(lpIOContext->client);
                delete lpIOContext;
                continue;
            }
        }
    }
    return 0;
}
