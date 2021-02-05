// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <Windows.h>
#include "resource.h"
#include <stdio.h>
#include <WINSOCK2.H>
#include <ctime> 
#include <stdlib.h>
#include "cJSON.h"
#include <atlstr.h>
#include "socketTool.h"
#include "ReceiveMessage.h"
#include "SendMessage.h"
#include "utils.h"

using namespace std;

SOCKET Global_Client = 0;

// 函数声明
VOID hold_the_socket();
VOID Listen_to_Server();



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		while (Global_Client == 0)
		{
			Global_Client = Connect_to_Server(); //启动连接服务器
			if (Global_Client == 0)
				MessageBox(NULL, L"首次连接Python server失败", L"Connect server error", 0);
		}
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StartHookWeChat, NULL, NULL, 0);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hold_the_socket, NULL, NULL, 0);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Listen_to_Server, NULL, NULL, 0);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

//监听python server端发来的数据
VOID Listen_to_Server()
{
	SOCKET client = 0;
	DWORD ret = 0;
	DWORD code = 0;
	while (true) {
		client = Global_Client;
		if (client)
		{
			CHAR recData[0x2000] = { 0 };
			wchar_t wxid[0x100] = { 0 };
			wchar_t at_wxid[0x100] = { 0 };
			wchar_t message[0x1000] = { 0 };
			ret = recv(client, recData, sizeof(recData), 0);
			if (strlen(recData) != 0) {
				recData[strlen(recData)] = 0x00;
				//把utf-8编码字符串转成Unicode编码字符串
				wchar_t* buff = UTF8ToUnicode(recData);
				cJSON* json;
				json = cJSON_Parse(buff); //把Unicode字符串解析成json形式数据
				//获取消息类型code
				code = cJSON_GetObjectItem(json, L"code")->valueint;
				switch (code)
				{
					//发送文本消息
				case 1:
					//获取微信id
					swprintf_s(wxid, L"%s", cJSON_GetObjectItem(json, L"wxid")->valuestring);
					//wchar_t * wxid = cJSON_GetObjectItem(json, L"wxid")->valuestring;
					//获取@微信id
					swprintf_s(at_wxid, L"%s", cJSON_GetObjectItem(json, L"at_wxid")->valuestring);
					//wchar_t * at_wxid = cJSON_GetObjectItem(json, L"at_wxid")->valuestring;
					//获取微信id
					swprintf_s(message, L"%s", cJSON_GetObjectItem(json, L"content")->valuestring);
					//wchar_t * message = cJSON_GetObjectItem(json, L"content")->valuestring;
					//发送文本消息
					sendTextMessage(wxid, at_wxid, message);
					break;
					//获取个人信息
				case 2:
					break;
				default:
					break;
				}
				cJSON_Delete(json);
			}
		}
	}
}


//维持微信与server的socket连接
VOID hold_the_socket()
{
	wchar_t buff[0x100] = { 0 };
	wchar_t type[0x100] = L"200";
	//获取微信进程pid
	CHAR pid_str[0x100] = { 0 };
	wchar_t processPid[0x100] = { 0 };
	DWORD PID = GetCurrentProcessId();
	// 把DWORD(即int)类型转成wchat_t类型
	_itoa_s(PID, pid_str, 10);
	//get_process_pid(processPid); //获取微信进程pid， GetCurrentProcessId不能在其他文件调用
	swprintf(processPid, sizeof(processPid), L"%hs", pid_str);
	swprintf_s(buff, L"{\"pid\":%s,\"type\":%s}*88888888*", processPid, type);
	const char* sendData = UnicodeToChar(buff);  //将Unicode编码转成CHAR类型，用于socket传输
	SOCKET client = 0;
	while (true)
	{
		client = Global_Client;
		//send()用来将数据由指定的socket传给对方主机
		//int send(int s, const void * msg, int len, unsigned int flags)
		//s为已建立好连接的socket，msg指向数据内容，len则为数据长度，参数flags一般设0
		//成功则返回实际传送出去的字符数，失败返回-1，错误原因存于error 
		int send_result = send(client, sendData, strlen(sendData), 0);
		if (send_result == -1)  //发送失败
		{
			closesocket(Global_Client);
			Global_Client = 0;
			while (Global_Client == 0)
			{
				Global_Client = Connect_to_Server(); //启动连接服务器，连接失败返回0
				Sleep(1 * 1000);  //延时1s
				//MessageBox(NULL, L"连接Python server失败", L"Connect server error", 0);
			}
		}
		Sleep(2 * 1000);  //延时2s
	}
}