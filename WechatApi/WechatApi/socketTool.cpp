// 用于socket通信的相关函数
#include "pch.h"
#include <WINSOCK2.H>
#include "utils.h"


//连接服务器
SOCKET Connect_to_Server()
{
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET)
	{
		//MessageBox(NULL, L"Invalid socket!", L"Starts Error", 0);
		return 0;
	}

	sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(9527);
	serAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	if (connect(client, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{  //连接失败 
		//MessageBox(NULL, L"Python server hasn't been opened yet!", L"Connect Error", 0);
		closesocket(client);
		return 0;
	}
	return client;
}


