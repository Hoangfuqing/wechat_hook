#include "pch.h"
#include <Windows.h>
#include <stdio.h>
#include "utils.h"

// ΢����Ϣ�Ľṹ��
struct wxStr
{
	wchar_t* pStr;
	int strLen;
	int strLen2;
};

VOID sendTextMessage(wchar_t* wxid, wchar_t* at_wxid, wchar_t* message)
{
	DWORD sendCall = getWechatWin() + 0x38D8A0;

	wxStr pWxid = { 0 };
	pWxid.pStr = wxid;
	pWxid.strLen = wcslen(wxid);
	pWxid.strLen2 = wcslen(wxid) * 2;

	wxStr at_pWxid = { 0 };
	char* asmAt_Wxid = 0x0;

	wxStr pMessage = { 0 };
	pMessage.pStr = message;
	pMessage.strLen = wcslen(message);
	pMessage.strLen2 = wcslen(message) * 2;

	//ȡ��ַ
	char* asmWxid = (char*)&pWxid.pStr;
	char* asmMessage = (char*)&pMessage.pStr;

	char buff[0x880] = { 0 };
	__asm
	{
		mov edx, asmWxid;
		//mov eax, asmAt_Wxid
		push 0x1;
		mov edi, 0x0;
		push edi;
		mov ebx, asmMessage;
		push ebx;
		lea ecx, buff;
		call sendCall;
		add esp, 0xC;
	}
}