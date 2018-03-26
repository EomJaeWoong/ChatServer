// ChatServer.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"

int _tmain(int argc, _TCHAR* argv[])
{
	CConfigData::CConfigData();

	CChatServer ChatServer;

	ChatServer.Start(CConfigData::m_Network_Chat_BindIP,
		CConfigData::m_Network_Chat_Bind_Port,
		CConfigData::m_System_Worker_Thread_Num,
		false,
		CConfigData::m_System_Max_Client
		);

	while (1)
	{
		system("cls");

		wprintf(L"==============================================================================\n");
		wprintf(L"                          ChatServer Monitoring\n");
		wprintf(L"==============================================================================\n\n");
		wprintf(L" - NetWork\n");
		wprintf(L"------------------------------------------------------------------------------\n\n");
		wprintf(L" Accept TPS :		%10d	Accept Total :		%10d\n", ChatServer._lAcceptTPS, ChatServer._lAcceptTotalTPS);
		wprintf(L" SendPacket TPS :	%10d	RecvPacket TPS :	%10d\n", ChatServer._lSendPacketTPS, ChatServer._lRecvPacketTPS);
		wprintf(L" Session Count :	%10d	PacketPool :		%10d\n\n", ChatServer.GetSessionCount(), ChatServer._lPacketPoolTPS);
		wprintf(L"------------------------------------------------------------------------------\n\n");
		wprintf(L" - Content\n");
		wprintf(L"------------------------------------------------------------------------------\n\n");
		wprintf(L" Update TPS :		%10d\n", ChatServer._lUpdateTPS);
		wprintf(L" LoginSessionKey :	%10d\n", 0);
		wprintf(L" UpdateMessage_Pool :	%10d	UpdateMessage_Queue :	%10d\n", ChatServer._lUpdateMessagePoolTPS, ChatServer._lUpdateMessageQueueTPS);
		wprintf(L" Player Count	:	%10d	PlayerData_Pool :	%10d\n", ChatServer._lPlayerCount, ChatServer._lPlayerPoolTPS);
		wprintf(L" Session Miss	:	%10d	Session Not Fount :	%10d", 0, 0);	

		Sleep(999);
	}

	return 0;
}

