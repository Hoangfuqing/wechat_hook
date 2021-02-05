#include "pch.h"
#include <Windows.h>
#include <stdio.h>
#include "resource.h"
#include <TlHelp32.h>
#include "utils.h"
#include <stdlib.h>
#include <WINSOCK2.H>
#include "socketTool.h"
#pragma comment(lib, "ws2_32.lib")
#define HOOK_LEN 5


HANDLE hWHND = 0;
DWORD WinAdd = 0;
HWND hDlg = 0;
DWORD hookAdd = 0;
DWORD retAdd = 0;
BYTE backCode[HOOK_LEN] = { 0 };

DWORD cEax = 0;
DWORD cEcx = 0;
DWORD cEdx = 0;
DWORD cEbx = 0;
DWORD cEsp = 0;
DWORD cEbp = 0;
DWORD cEsi = 0;
DWORD cEdi = 0;

// 数据发送到socket客户端
VOID send_to_py_server(DWORD msgAdd);

VOID send_to_py_server(DWORD msgAdd)
{
	//信息块的位置
	DWORD* msgAddress = (DWORD*)msgAdd;
	DWORD wxidAdd = (*msgAddress + 0x40);
	DWORD wxid2Add = (*msgAddress + 0x150);
	DWORD messageAdd = (*msgAddress + 0x68);
	DWORD msg_type_add = (*msgAddress + 0x30);
	DWORD self_add = (*msgAddress + 0x34);

	wchar_t msg_type[0x100] = { 0 };
	wchar_t self[0x100] = { 0 };

	DWORDToUnicode(*((DWORD*)msg_type_add), msg_type);
	DWORDToUnicode(*((DWORD*)self_add), self); 

	wchar_t buff[0x2000] = { 0 };
	wchar_t type[0x100] = L"1"; 
	
	CHAR pid_str[0x100] = { 0 };
	DWORD PID = GetCurrentProcessId(); 

	_itoa_s(PID, pid_str, 10);
	wchar_t processPid[0x100] = { 0 };
	swprintf(processPid, sizeof(processPid), L"%hs", pid_str);
	
	if (*(LPVOID*)wxid2Add <= 0x0) {
		swprintf_s(buff, L"{\"pid\":%s,\"self\":%s,\"type\":%s,\"msg_type\":%s,\"chatroom_ID\":\"%s\",\"wx_ID\":\"%s\",\"content\":\"%s\"}*88888888*",
			processPid, self, type, msg_type, L"", *((LPVOID*)wxidAdd), *((LPVOID*)messageAdd));
	}
	else {
		swprintf_s(buff, L"{\"pid\":%s,\"self\":%s,\"type\":%s,\"msg_type\":%s,\"chatroom_ID\":\"%s\",\"wx_ID\":\"%s\",\"content\":\"%s\"}*88888888*",
			processPid, self, type, msg_type, *((LPVOID*)wxidAdd), *((LPVOID*)wxid2Add), *((LPVOID*)messageAdd));
	}
	if (Global_Client == 0)
		return;
	const char* sendData = UnicodeToChar(buff); 
	send(Global_Client, sendData, strlen(sendData), 0);
}


//跳转过来的函数 我们自己的
VOID __declspec(naked) HookF()
{
	__asm {
		mov cEax, eax
		mov cEcx, ecx
		mov cEdx, edx
		mov cEbx, ebx
		mov cEsp, esp
		mov cEbp, ebp
		mov cEsi, esi
		mov cEdi, edi

		pushad
		pushfd
	}
	//然后跳转到我们自己的处理函数 想干嘛干嘛
	send_to_py_server(cEsi);
	retAdd = WinAdd + 0x3BA682;
	__asm {
		popfd
		popad
		jmp retAdd
	}
}

VOID StartHook(DWORD hookAdd, LPVOID jmpAdd)
{
	BYTE JmpCode[HOOK_LEN] = { 0 };
	JmpCode[0] = 0xE9;
	*(DWORD*)&JmpCode[1] = (DWORD)jmpAdd - hookAdd - HOOK_LEN;
	if (ReadProcessMemory(hWHND, (LPVOID)hookAdd, backCode, HOOK_LEN, NULL) == 0) {
		return;
	}
	if (WriteProcessMemory(hWHND, (LPVOID)hookAdd, JmpCode, HOOK_LEN, NULL) == 0) {
		return;
	}
}

VOID StartHookWeChat()
{
	//消息接收 3.0.0.57 0x3BA67D
	hookAdd = getWechatWin() + 0x3BA67D;
	hWHND = OpenProcess(PROCESS_ALL_ACCESS, NULL, GetCurrentProcessId());
	WinAdd = getWechatWin();
	StartHook(hookAdd, &HookF);
}