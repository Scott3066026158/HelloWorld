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
ulong_ptr internal; //����ϵͳ������ָ��һ����ϵͳ��ص�״̬
ulong_ptr internalhigh; //ָ�����ͻ���յ����ݳ���
union {
struct {
dword offset; //�ļ����͵��ֽ�ƫ�����ĵ�λ��
dword offsethigh; //�ļ����͵��ֽ�ƫ�����ĸ�λ��
};
pvoid pointer; //ָ�룬ָ���ļ�����λ��
};
handle hevent; //ָ��һ��i/o������ɺ󴥷����¼�
} OVERLAPPED, *LPOVERLAPPED;
*/

enum IO_OPERATION{IO_READ,IO_WRITE};


//typedef struct _WSABUF {
//    ULONG len;								���鳤��
//    _Field_size_bytes_(len) CHAR FAR *buf;    char*
//} WSABUF, FAR * LPWSABUF;

struct IO_DATA{
    OVERLAPPED                  Overlapped;  //
    WSABUF                      wsabuf;
    IO_OPERATION                opCode;     //��д��־
    SOCKET                      client;
};
HANDLE h_IOCP;
char buffer[1024];
int main()
{
	int result = server();
	if(result == -1)
	{
		printf("��������ʧ�ܣ�\n");
	}
	system("pause");
	return 0;
}
int server()
{
	//���հ汾��Ϣ
    WSADATA socketVersion;
	//��ȡ�汾��Ϣ
    int result = WSAStartup(MAKEWORD(2,2), &socketVersion);
	if(result != 0)
	{
		printf("socket���ʧЧ��\n");
		return -1;
	}
	//af��[in]һ����ַ��淶��Ŀǰ��֧��AF_INET��ʽ��
	//type�����׽ӿڵ�����������
	//protocol���׽ӿ�ʹ�õ��ض�Э�飬��������߲�Ըָ��Э����Ϊ0��
	//lpProtocolInfo��һ��ָ��PROTOCOL_INFO�ṹ��ָ�룬�ýṹ�����������׽ӿڵ����ԡ�������������㣬��ǰ����������af, type, protocol�������ԡ�
	//g��������δ��ʹ�õ��׽����顣�׽ӿ���ı�ʶ����
	//iFlags���׽ӿ�����������
	//return��IOsocket������
    UINT m_socket = WSASocket(AF_INET,SOCK_STREAM, IPPROTO_TCP, 0,0,WSA_FLAG_OVERLAPPED);
	if(m_socket == -1)
	{
		printf("Socket����ʧ�ܣ�\n");
		return -1;
	}
	//���������ĵ�ַ��Ϣ
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(9990);
    server.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//��
    result = bind(m_socket ,(sockaddr*)&server,sizeof(server));
	if(result == -1)
	{
		printf("Socket��ʧ�ܣ�\n");
		return -1;
	}
	//��ʼ����
    result = listen(m_socket, 5);
	if(result == -1)
	{
		printf("Socket����ʧ�ܣ�\n");
		return -1;
	}
	//�����洢ϵͳ��Ϣ�Ľṹ��
    SYSTEM_INFO sysInfo;
	//��ȡϵͳ��Ϣ
    GetSystemInfo(&sysInfo);
	//������ = �������� * 2(Ϊ�β��ʹ�����������һ����Ϊ�˸��õ�����ϵͳ����)
    int ConcurrentCount = sysInfo.dwNumberOfProcessors * 2;
	//����IOCP,����һ�����
    h_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,ConcurrentCount);
	//��������ָ����WorkerThread
    for(int i = 0;i < ConcurrentCount; ++i){
        DWORD   dwThreadId;
        HANDLE  hThread = CreateThread(NULL, 0, WorkerThread, 0, 0, &dwThreadId);
        CloseHandle(hThread);
    }
	//ѭ�����տͻ��˵�����
    while(1)
    {
		printf("�ȴ�����...\n");
		//��������
        SOCKET client = accept(m_socket, NULL, NULL);
        printf("�ѽ��տͻ��˵�����");
		//û����һ��Socket�ͽ����IOCP��
        if (CreateIoCompletionPort((HANDLE)client, h_IOCP, 0, 0) == NULL)
		{
			printf("��IOCP��ͻ������ӵ�Socket��ʧ�ܣ�ʧ�ܴ��룺%u\n",GetLastError());
            closesocket(client);
        }
        else 
		{
			//����һ���洢���ͻ����������Ϣ�Ľṹ��
            IO_DATA* data = new IO_DATA;
			//��ʼ��һ��char����
            memset(buffer, 0 ,1024);
			//��ʼ���ص��ṹ��(��ʵ�ص��Ҿ��ý��첽�������)
            memset(&data->Overlapped, 0 , sizeof(data->Overlapped));
			//��ʼ�趨�Ƕ�ȡ����
            data->opCode = IO_READ;
			//��ʼ��WSABUF
            data->wsabuf.buf  = buffer;
            data->wsabuf.len  = sizeof(buffer);
			//�����յ�Socket����������data
            data->client = client;
			//��ʼ���������ݵĴ�С
            DWORD nBytes = 1024;
			//��ʼ���������ݵĴ�С
			DWORD dwFlags = 0;
			/*
			s��һ����ʶ�������׽ӿڵ������֡�
			lpBuffers��һ��ָ��WSABUF�ṹ�����ָ�롣ÿһ��WSABUF�ṹ����һ����������ָ��ͻ������ĳ��ȡ�
			dwBufferCount��lpBuffers������WSABUF�ṹ����Ŀ��
			lpNumberOfBytesRecvd��������ղ�������������һ��ָ�򱾵��������յ��ֽ�����ָ�롣
			lpFlags��һ��ָ���־λ��ָ�롣
			lpOverlapped��һ��ָ��WSAOVERLAPPED�ṹ��ָ�루���ڷ��ص��׽ӿ�����ԣ���
			lpCompletionRoutine��һ��ָ����ղ�����������õ����̵�ָ�루���ڷ��ص��׽ӿ�����ԣ���
			*/
            int nRet = WSARecv(client, &data->wsabuf, 1, &nBytes, &dwFlags, &data->Overlapped, NULL);
            if(nRet == SOCKET_ERROR  && ERROR_IO_PENDING != WSAGetLastError()){
				printf("�����ϵͳͶ�ݽ��ղ���ʧ�ܣ�ԭ�����:%d\n", WSAGetLastError());
                closesocket(client);
                delete data;
            }
			//������յ�����Ϣ
			printf("%s\n", data->wsabuf.buf);
        }
    }
    closesocket(m_socket);
    WSACleanup();
}

DWORD WINAPI WorkerThread (LPVOID WorkThreadContext) {
	
	//���մ���ɶ������ȡ�����ݵ�ַ
    LPOVERLAPPED lpOverlapped = NULL;
    IO_DATA *lpIOContext = NULL; 	
	//���ݳ���
    DWORD dwIoSize = 0;
	//���մ����߳�ʱ�����key
    void* lpCompletionKey = NULL;
	//����������ݵĴ�С
	DWORD nBytes = 0;
	//��ʶλ
	DWORD dwFlags = 0;
	//Ͷ�ݺ�������ݵĴ�С
	int nRet = 0;
    while(1)
	{
		//ѭ��ѯ����ɶ�������û����ɵĲ���
        GetQueuedCompletionStatus(h_IOCP, &dwIoSize,(LPDWORD)&lpCompletionKey,&lpOverlapped, INFINITE);
        lpIOContext = (IO_DATA*)lpOverlapped;
		//������/������ɶ��������ݴ�СΪ0����رտͻ��˵�����
        if(dwIoSize == 0)
        {
			printf("����ɶ����л�ȡԪ��ʧ�ܣ�ԭ�����:%d\n", WSAGetLastError());
			printf("�˿ͻ��������ѶϿ�:%u\n", lpIOContext->client);
            closesocket(lpIOContext->client);
            delete lpIOContext;
            continue;
        }
		//����ɶ��ж�ȡ����opcode��read�Ļ�
        if(lpIOContext->opCode == IO_READ)
        {
			//��ӡ�Ѿ���ɽ��յ�����
			printf("�������յ�����Ϣ��%s\n", lpIOContext->wsabuf.buf);
			//��ջ���
            ZeroMemory(&lpIOContext->Overlapped, sizeof(lpIOContext->Overlapped));
			//����д�뽫Ҫ���͵�����
            lpIOContext->wsabuf.buf = "���Ƿ�����";
			//���÷������ݴ�С
            lpIOContext->wsabuf.len = strlen(buffer)+1;
            nBytes = lpIOContext->wsabuf.len;
			//�ı��ʶ
            lpIOContext->opCode = IO_WRITE;
			//��ʶ��0
            dwFlags = 0;
			//��������ϵͳͶ��һ����������
            nRet = WSASend(lpIOContext->client, &lpIOContext->wsabuf, 1, &nBytes, dwFlags, &(lpIOContext->Overlapped), NULL);
			//��������Ͷ��ʧ�ܣ���رմ�Socket����
            if(nRet == SOCKET_ERROR && ERROR_IO_PENDING != WSAGetLastError() ) 
			{
				printf("�����ϵͳͶ�ݷ��Ͳ���ʧ�ܣ�ԭ�����:%d\n", WSAGetLastError());
				printf("�˿ͻ��������ѶϿ�:%u\n", lpIOContext->client);
                closesocket(lpIOContext->client);
                delete lpIOContext;
                continue;
            }
			//��ջ���
            memset(buffer, NULL, sizeof(buffer));
        }
		//����ɶ��ж�ȡ����opcode��read�Ļ�
        else if(lpIOContext->opCode == IO_WRITE)
        {
			//��ӡ�Ѿ���ɽ��յ�����
			printf("���������͵���Ϣ��%s\n", lpIOContext->wsabuf.buf);
			//�ı��ʶ
            lpIOContext->opCode = IO_READ; 
			//���ý������ݴ�С
            nBytes = 1024;
			//��ʶ��0
            dwFlags = 0;
			//���ý������ݵ�����
            lpIOContext->wsabuf.buf = buffer;
			//���ý������ݵ�����Ĵ�С
            lpIOContext->wsabuf.len = nBytes;
			//��ջ���
            ZeroMemory(&lpIOContext->Overlapped, sizeof(lpIOContext->Overlapped));
			//��������ϵͳͶ��һ����������
            nRet = WSARecv(lpIOContext->client,&lpIOContext->wsabuf, 1, &nBytes, &dwFlags, &lpIOContext->Overlapped, NULL);
            if(nRet == SOCKET_ERROR && ERROR_IO_PENDING != WSAGetLastError()) 
			{
                printf("�����ϵͳͶ�ݽ��ղ���ʧ�ܣ�ԭ�����:%d\n", WSAGetLastError());
				printf("�˿ͻ��������ѶϿ�:%u\n", lpIOContext->client);
                closesocket(lpIOContext->client);
                delete lpIOContext;
                continue;
            }
        }
    }
    return 0;
}
