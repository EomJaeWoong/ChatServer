#include "stdafx.h"

CChatServer::CChatServer() : CNetServer()
{
	_hUpdateEvent	= CreateEvent(NULL, FALSE, FALSE, NULL);
	_bShutdown		= false;
	
	///////////////////////////////////////////////////////////////////////////////////////
	// ChatServer Monitor 스레드
	///////////////////////////////////////////////////////////////////////////////////////
	DWORD dwThreadID;
	_hMonitorThreadChat = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread_Chat,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	SYSLOG_DIRECTORY(L"ChatServerLog");
	SYSLOG_LEVEL(LOG::LEVEL_DEBUG);
}


CChatServer::~CChatServer()
{
	CloseHandle(_hUpdateEvent);
}


bool						CChatServer::Start(WCHAR* wOpenIP, int iPort, int iWorkerThreadNum, bool bNagle, int iMaxConnect)
{
	if (!CNetServer::Start(wOpenIP, iPort, iWorkerThreadNum, bNagle, iMaxConnect))
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// Update 스레드
	///////////////////////////////////////////////////////////////////////////////////////
	DWORD dwThreadID;
	_hUpdateThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		UpdateThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"ChatServer Start");
	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"Open IP : %s", wOpenIP);
	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"Port : %d", iPort);
	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"WorkerThread Count : %d", iWorkerThreadNum);
	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"Nagle : %s", bNagle ? L"TRUE" : L"FALSE");
	LOG(L"ChatServer", LOG::LEVEL_SYSTEM, L"Max Connection : %d", iMaxConnect);

	return true;
}


void						CChatServer::Stop()
{
	int iResult;

	CNetServer::Stop();

	_bShutdown = true;
	iResult = WaitForSingleObject(_hUpdateThread, INFINITE);
	if (iResult == WAIT_OBJECT_0)
		return;
}


/*-----------------------------------------------------------------------------*/
// Handler
/*-----------------------------------------------------------------------------*/
void						CChatServer::OnClientJoin(SESSIONINFO *pSessionInfo, __int64 iSessionID)
{
	MESSAGE *pMessage = MakeMessage_NewConnection(iSessionID);
	_MessageQueue.Put(pMessage);
	InterlockedIncrement(&_lUpdateMessageQueueCounter);
	SetEvent(_hUpdateEvent);
}

void						CChatServer::OnClientLeave(__int64 iSessionID)
{
	MESSAGE *pMessage = MakeMessage_Disconnection(iSessionID);
	_MessageQueue.Put(pMessage);
	InterlockedIncrement(&_lUpdateMessageQueueCounter);
	SetEvent(_hUpdateEvent);
}

bool						CChatServer::OnConnectionRequest(SESSIONINFO *pSessionInfo)
{
	return true;
}

void						CChatServer::OnRecv(__int64 iSessionID, CNPacket* pRecvPacket)
{
	MESSAGE *pMessage = MakeMessage_Packet(iSessionID, pRecvPacket);
	_MessageQueue.Put(pMessage);
	InterlockedIncrement(&_lUpdateMessageQueueCounter);
	SetEvent(_hUpdateEvent);
}

void						CChatServer::OnSend(__int64 iSessionID, int iSendsize)
{

}

void						CChatServer::OnWorkerThreadBegin()
{

}

void						CChatServer::OnWorkerThreadEnd()
{

}

void						CChatServer::OnError(int iErrorCode, WCHAR *wErrorMsg)
{

}



/*-----------------------------------------------------------------------------*/
// Update Thread
/*-----------------------------------------------------------------------------*/
int							CChatServer::UpdateThread_update()
{
	MESSAGE		*pMessage = nullptr;
	int			iResult;

	while (!_bShutdown)
	{
		iResult = WaitForSingleObject(_hUpdateEvent, INFINITE);
		if (WAIT_OBJECT_0 != iResult)
			CCrashDump::Crash();

		while (!_MessageQueue.isEmpty())
		{
			if (!_MessageQueue.Get(&pMessage))
				return false;

			if (!CompleteMessage(pMessage))
			{
				LOG(L"ChatServer_Error",
					LOG::LEVEL_ERROR,
					L"Message Process Error : [Message type : %d][Session ID : %p]",
					pMessage->wType, pMessage->iSessionID);
				CCrashDump::Crash();
			}

			InterlockedIncrement(&_lUpdateCounter);
		}
	}

	return 0;
}

unsigned __stdcall			CChatServer::UpdateThread(LPVOID updateParam)
{
	return ((CChatServer *)updateParam)->UpdateThread_update();
}



/*-----------------------------------------------------------------------------*/
// 메시지 
/*-----------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////
// 메시지 처리
/////////////////////////////////////////////////////////////////////////////////
bool						CChatServer::CompleteMessage(MESSAGE *pMessage)
{
	bool		bResult = true;
	
	/////////////////////////////////////////////////////////////////////////////
	// e_MESSAGE_NEW_CONNECTION	- 신규 접속
	// e_MESSAGE_DISCONNECTION	- 접속 해제
	// e_MESSAGE_PACKET			- 패킷 처리
	/////////////////////////////////////////////////////////////////////////////
	switch (pMessage->wType)
	{
	case e_MESSAGE_NEW_CONNECTION:
		bResult = CreateClient(pMessage->iSessionID);
		break;

	case e_MESSAGE_DISCONNECTION:
		bResult = DeleteClient(pMessage->iSessionID);
		break;

	case e_MESSAGE_PACKET:
		bResult = CompletePacket(pMessage->iSessionID, pMessage->pPacket);
		break;

	default:
		Disconnect(pMessage->iSessionID);
		bResult = false;

		LOG(L"ChatServer_Error", 
			LOG::LEVEL_ERROR,
			L"CompleteMessage - Message Type Error [Type : %d]",
			pMessage->wType);

		break;

	}

	_MessageMemoryPool.Free(pMessage);

	return bResult;
}

/////////////////////////////////////////////////////////////////////////////////
// 메시지 제작
/////////////////////////////////////////////////////////////////////////////////
CChatServer::MESSAGE*		CChatServer::MakeMessage_NewConnection(__int64 iSessionID)
{
	MESSAGE *pOutMessage = _MessageMemoryPool.Alloc();

	pOutMessage->wType = e_MESSAGE_NEW_CONNECTION;

	pOutMessage->iSessionID = iSessionID;
	pOutMessage->pPacket = nullptr;

	return pOutMessage;
}

CChatServer::MESSAGE*		CChatServer::MakeMessage_Disconnection(__int64 iSessionID)
{
	MESSAGE *pOutMessage = _MessageMemoryPool.Alloc();

	pOutMessage->wType = e_MESSAGE_DISCONNECTION;

	pOutMessage->iSessionID = iSessionID;
	pOutMessage->pPacket = nullptr;

	return pOutMessage;
}

CChatServer::MESSAGE*		CChatServer::MakeMessage_Packet(__int64 iSessionID, CNPacket *pPacket)
{
	MESSAGE *pOutMessage = _MessageMemoryPool.Alloc();

	pOutMessage->wType = e_MESSAGE_PACKET;

	pOutMessage->iSessionID = iSessionID;
	pOutMessage->pPacket = pPacket;

	return pOutMessage;
}



/*-----------------------------------------------------------------------------*/
// 패킷 처리, 제작
/*-----------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////
// 패킷 분류
/////////////////////////////////////////////////////////////////////////////////
bool						CChatServer::CompletePacket(__int64 iSessionID, CNPacket *pRecvPacket)
{
	bool bResult = true;
	WORD wType = 0xffff;
	
	*pRecvPacket >> wType;

	/////////////////////////////////////////////////////////////////////////////
	// en_PACKET_CS_CHAT_REQ_LOGIN			- 로그인 요청
	// en_PACKET_CS_CHAT_REQ_SECTOR_MOVE	- 섹터 이동 요청
	// en_PACKET_CS_CHAT_REQ_MESSAGE		- 메시지 요청
	// en_PACKET_CS_CHAT_REQ_HEARTBEAT		- 하트비트
	/////////////////////////////////////////////////////////////////////////////
	switch (wType)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		bResult = PacketProc_ReqLogin(iSessionID, pRecvPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		bResult = PacketProc_ReqSectorMove(iSessionID, pRecvPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		bResult = PacketProc_ReqMessage(iSessionID, pRecvPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		bResult = PacketProc_ReqHeartbeat(iSessionID, pRecvPacket);
		break;

	default:
		Disconnect(iSessionID);
		bResult = false;

		LOG(L"ChatServer_Error", 
			LOG::LEVEL_ERROR,
			L"CompletePacket - Packet Type Error");
		LOG_HEX(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			pRecvPacket->GetBufferPtr(),
			pRecvPacket->GetDataSize());
		break;
	}

	pRecvPacket->Free();

	return bResult;
}

////////////////////////////////////////////////////////////////////////////////
// 패킷 처리
////////////////////////////////////////////////////////////////////////////////
bool						CChatServer::PacketProc_ReqLogin(__int64 iSessionID, CNPacket *pRecvPacket)
{
	CNPacket	*pSendPacket = nullptr;
	CLIENT		*pClient = SearchClient(iSessionID);

	__int64		iAccountNo;
	BYTE		byStatus = 1;

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////	
	*pRecvPacket >> iAccountNo;

	////////////////////////////////////////////////////////////////////////////
	// 클라이언트 찾기
	////////////////////////////////////////////////////////////////////////////
	if (nullptr == pClient)
	{
		LOG(L"ChatServer_Debug",
			LOG::LEVEL_DEBUG,
			L"PacketProc_ReqLogin - Cannot find Client [Session ID : %p][Account No : %d]");
		byStatus = 0;
	}

	else
	{
		////////////////////////////////////////////////////////////////////////////
		// 원래 로그인 되어있던 Client 끊기
		////////////////////////////////////////////////////////////////////////////
		ClientIter iter;
		for (iter = _Client.begin(); iter != _Client.end(); iter++)
		{
			if ((iter->second->iAccountNo == iAccountNo) && (iter->first != iSessionID))
			{
				Disconnect(iter->first);
				iter->second->bDisconnect = TRUE;
				
				break;
			}
		}

		pClient->dwRecvPacketTime = GetTickCount64();

		////////////////////////////////////////////////////////////////////////////
		// 새로 생성 되었던 Client 찾아서 Account 넣어줘야 됨
		////////////////////////////////////////////////////////////////////////////
		pClient->iAccountNo = iAccountNo;

		////////////////////////////////////////////////////////////////////////////
		// ID, Nickname 확인
		////////////////////////////////////////////////////////////////////////////
		pRecvPacket->GetData((unsigned char *)pClient->szID, eMAX_ID * sizeof(WCHAR));
		pRecvPacket->GetData((unsigned char *)pClient->szNickname, eMAX_NICKNAME * sizeof(WCHAR));


		// 세션키 확인해야 하는데 지금은 그냥 믿고 감
		pRecvPacket->GetData((unsigned char *)pClient->chSessionKey, 64);

		pClient->bLogin = TRUE;

		InterlockedIncrement(&_lPlayerCount);

	}

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResLogin(pClient->iAccountNo, byStatus);
	SendPacket_One(pClient, pSendPacket);

	pSendPacket->Free();

	return true;
}

bool						CChatServer::PacketProc_ReqSectorMove(__int64 iSessionID, CNPacket *pRecvPacket)
{
	CNPacket	*pSendPacket = nullptr;
	CLIENT		*pClient = SearchClient(iSessionID);

	__int64		iAccountNo;
	short		shSectorX = -1, shSectorY = -1;

	if (!pClient->bLogin)
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqSectorMove - Not Login [Client Account No : %d][Session ID : %p]",
			pClient->iAccountNo, iSessionID);
		CCrashDump::Crash();
		//return false;
	}

	pClient->dwRecvPacketTime = GetTickCount64();

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////
	*pRecvPacket >> iAccountNo;
	if (iAccountNo != pClient->iAccountNo)
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqSectorMove - Account not equil [Recv Account No : %d][Client Account No : %d][Session ID : %p]",
			iAccountNo, pClient->iAccountNo, iSessionID);
		CCrashDump::Crash();
		//return false;
	}

	////////////////////////////////////////////////////////////////////////////
	// 섹터 뽑아서 검사후 넣고 섹터 업데이트
	////////////////////////////////////////////////////////////////////////////
	*pRecvPacket >> shSectorX;
	*pRecvPacket >> shSectorY;

	////////////////////////////////////////////////////////////////////////////
	// 섹터 업데이트
	////////////////////////////////////////////////////////////////////////////
	if (!UpdateSector(pClient, shSectorX, shSectorY))
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqSectorMove - Sector Error : [Account No : %d][Session ID : %p][ClientX : %d][ClientY : %d][X : %d][Y : %d]",
			iAccountNo, pClient->iAccountNo, iSessionID, pClient->shSectorX, pClient->shSectorY, shSectorX, shSectorY);
		CCrashDump::Crash();
	}

	if (pClient->shSectorX == -1 || pClient->shSectorY == -1 || shSectorX != pClient->shSectorX || shSectorY != pClient->shSectorY)
		CCrashDump::Crash();

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResSectorMove(pClient->iAccountNo, pClient->shSectorX, pClient->shSectorY);
	SendPacket_One(pClient, pSendPacket);

	pSendPacket->Free();

	return true;
}

bool						CChatServer::PacketProc_ReqMessage(__int64 iSessionID, CNPacket *pRecvPacket)
{
	CNPacket	*pSendPacket = nullptr;
	CLIENT		*pClient = SearchClient(iSessionID);

	__int64		iAccountNo;
	WORD		wMessageLen = 0;
	WCHAR		szMessage[1024];

	if (!pClient->bLogin ||
		(-1 == pClient->shSectorX || -1 == pClient->shSectorY))
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqMessage - Not Login [Client Account No : %d][Session ID : %p][X : %d][Y : %d]",
			pClient->iAccountNo, iSessionID, pClient->shSectorX, pClient->shSectorY);
		return false;
	}

	pClient->dwRecvPacketTime = GetTickCount64();

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////
	*pRecvPacket >> iAccountNo;
	if (iAccountNo != pClient->iAccountNo)
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqMessage - Account not equil [Recv Account No : %d][Client Account No : %d][Session ID : %p]",
			iAccountNo, pClient->iAccountNo, iSessionID);
		CCrashDump::Crash();
		//return false;
	}

	////////////////////////////////////////////////////////////////////////////
	// 메시지 길이 뽑고 메시지 뽑기
	////////////////////////////////////////////////////////////////////////////
	*pRecvPacket >> (WORD)wMessageLen;
	
	memset(szMessage, 0, 1024);
	int iMessageLen = pRecvPacket->GetData((unsigned char *)szMessage, wMessageLen);
	if (wMessageLen != iMessageLen)
	{
		LOG(L"ChatServer_Error",
			LOG::LEVEL_ERROR,
			L"PacketProc_ReqMessage - Message Length [Account No : %d][Session ID : %p][Message Length : %d][Recv Message Length : %d]",
			pClient->iAccountNo, iSessionID, wMessageLen, iMessageLen);
		CCrashDump::Crash();
	}

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResMessage(pClient->iAccountNo, pClient->szID, pClient->szNickname, wMessageLen, szMessage);
	SendPacket_Around(pClient, pSendPacket, true);

	pSendPacket->Free();

	return true;
}

bool						CChatServer::PacketProc_ReqHeartbeat(__int64 iSessionID, CNPacket *pRecvPacket)
{
	CLIENT		*pClient = SearchClient(iSessionID);
	if (nullptr == pClient)
		return true;
	
	pClient->dwRecvPacketTime = GetTickCount64();
	LOG(L"ChatServer_Debug",
		LOG::LEVEL_DEBUG,
		L"Heartbeat - [Session ID : %p][Account No : %d]",
		iSessionID, pClient->iAccountNo);
	return true;
}



/////////////////////////////////////////////////////////////////////////////////
// 패킷 제작
/////////////////////////////////////////////////////////////////////////////////
CNPacket*					CChatServer::MakePacket_ResLogin(__int64 iAccountNo, BYTE byStatus)
{
	CNPacket *pPacket = CNPacket::Alloc();
	if (nullptr == pPacket)
		return nullptr;

	*pPacket << (WORD)en_PACKET_CS_CHAT_RES_LOGIN;

	*pPacket << byStatus;
	*pPacket << iAccountNo;

	return pPacket;
}

CNPacket*					CChatServer::MakePacket_ResSectorMove(__int64 iAccountNo, short shSectorX, short shSectorY)
{
	CNPacket *pPacket = CNPacket::Alloc();
	if (nullptr == pPacket)
		return nullptr;

	*pPacket << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE;

	*pPacket << iAccountNo;
	*pPacket << shSectorX;
	*pPacket << shSectorY;

	return pPacket;
}

CNPacket*					CChatServer::MakePacket_ResMessage(__int64 iAccountNo, WCHAR *szID, WCHAR *szNickname, WORD wMessageLen, WCHAR *pMessage)
{
	CNPacket *pPacket = CNPacket::Alloc();
	if (nullptr == pPacket)
		return nullptr;

	*pPacket << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;

	*pPacket << iAccountNo;

	pPacket->PutData((unsigned char *)szID, eMAX_ID * 2);
	pPacket->PutData((unsigned char *)szNickname, eMAX_NICKNAME * 2);

	*pPacket << wMessageLen;

	pPacket->PutData((unsigned char *)pMessage, wMessageLen);

	return pPacket;
}


/////////////////////////////////////////////////////////////////////////////////
// Send
/////////////////////////////////////////////////////////////////////////////////
void						CChatServer::SendPacket_One(CLIENT *pClient, CNPacket *pPacket)
{
	SendPacket(pClient->iSessionID, pPacket);
}

void						CChatServer::SendPacket_Around(CLIENT *pClient, CNPacket *pPacket, bool bSendMe)
{
	st_SECTOR_AROUND stAroundSector;
	GetSectorAround(pClient->shSectorX, pClient->shSectorY, &stAroundSector);

	SectorIter iter;
	for (int iCnt = 0; iCnt < stAroundSector.iCount; iCnt++)
	{
		CLIENT * pSendClient = nullptr;
		SECTOR &Sector = _Sector[stAroundSector.Around[iCnt].iY][stAroundSector.Around[iCnt].iX];
		for (iter = Sector.begin(); iter != Sector.end(); iter++)
		{
			if (!bSendMe)
				continue;

			pSendClient = SearchClient(*iter);
			SendPacket_One(pSendClient, pPacket);
		}
	}
}

void						CChatServer::SendPacket_Broadcast(CNPacket *pPacket)
{
	CLIENT *pClient = nullptr;

	ClientIter iter;
	for (iter = _Client.begin(); iter != _Client.end(); iter++)
		SendPacket_One(iter->second, pPacket);
}


/////////////////////////////////////////////////////////////////////////////////
// 클라이언트 생성, 삭제
/////////////////////////////////////////////////////////////////////////////////
bool						CChatServer::CreateClient(__int64 iSessionID)
{
	CLIENT *pClient = _ClientMemoryPool.Alloc();
	if (nullptr == pClient)
		return false;
	
	pClient->bLogin = FALSE;

	pClient->iSessionID = iSessionID;

	pClient->iAccountNo = 0;

	memset(pClient->szID, 0, eMAX_ID);
	memset(pClient->szNickname, 0, eMAX_NICKNAME);
	memset(pClient->chSessionKey, 0, 64);

	pClient->shSectorX = -1;
	pClient->shSectorY = -1;

	pClient->bDisconnect = FALSE;
	pClient->dwRecvPacketTime = 0;

	_Client.insert(pair<__int64, CLIENT *>(iSessionID, pClient));

	return true;
}

bool						CChatServer::DeleteClient(__int64 iSessionID)
{
	CLIENT *pClient = nullptr;
	ClientIter iter = _Client.find(iSessionID);

	if (iter != _Client.end())
	{
		pClient = iter->second;

		if ((-1 != pClient->shSectorX) && (-1 != pClient->shSectorY))
			_Sector[pClient->shSectorY][pClient->shSectorX].remove(pClient->iSessionID);

		_ClientMemoryPool.Free(pClient);
		_Client.erase(iter);

		if (0 != pClient->iAccountNo)
			InterlockedDecrement(&_lPlayerCount);

		return true;
	}

	else
	{
		LOG(L"ChatServer", LOG::LEVEL_ERROR, L"DeleteClient - Cannot find Client [Session ID : %d]", iSessionID);
		CCrashDump::Crash();
	}

	return false;
}

CChatServer::CLIENT*			CChatServer::SearchClient(__int64 iSessionID)
{
	CLIENT *pOutClient = nullptr;
	ClientIter iter= _Client.find(iSessionID);

	if (iter != _Client.end())
		pOutClient = iter->second;
	
	return pOutClient;
}



/////////////////////////////////////////////////////////////////////////////////
// 주변 섹터 얻기
/////////////////////////////////////////////////////////////////////////////////
void						CChatServer::GetSectorAround(short shSectorX, short shSectorY, st_SECTOR_AROUND *pSectorAround)
{
	shSectorX--;
	shSectorY--;

	pSectorAround->iCount = 0;

	for (int iCntY = 0; iCntY < 3; iCntY++)
	{
		if (shSectorY + iCntY < 0 || shSectorY + iCntY >= eSECTOR_MAX_Y)
			continue;

		for (int iCntX = 0; iCntX < 3; iCntX++)
		{
			if (shSectorX + iCntX < 0 || shSectorX + iCntX >= eSECTOR_MAX_X)
				continue;

			pSectorAround->Around[pSectorAround->iCount].iX = shSectorX + iCntX;
			pSectorAround->Around[pSectorAround->iCount].iY = shSectorY + iCntY;
			pSectorAround->iCount++;
		}
	}
}

bool						CChatServer::UpdateSector(CLIENT *pClient, short shNewSectorX, short shNewSectorY)
{
	__int64 iSessionID = pClient->iSessionID;

	/////////////////////////////////////////////////////////////////////////////
	//Sector영역 벗어날 때
	/////////////////////////////////////////////////////////////////////////////
	if (shNewSectorX < 0 || shNewSectorX >= eSECTOR_MAX_X)		return false;
	if (shNewSectorY < 0 || shNewSectorY >= eSECTOR_MAX_Y)		return false;

	if ((pClient->shSectorX == shNewSectorX) && (pClient->shSectorY == shNewSectorY))
		return true;

	/////////////////////////////////////////////////////////////////////////////
	// 원래 있던 곳에서 지우기
	/////////////////////////////////////////////////////////////////////////////
	if ((pClient->shSectorX != -1) && (pClient->shSectorY != -1))
		_Sector[pClient->shSectorY][pClient->shSectorX].remove(iSessionID);

	_Sector[shNewSectorY][shNewSectorX].push_back(iSessionID);

	pClient->shSectorX = shNewSectorX;
	pClient->shSectorY = shNewSectorY;

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// ChatServer 모니터링 스레드
/////////////////////////////////////////////////////////////////////////////////
unsigned __stdcall			CChatServer::MonitorThread_Chat(LPVOID MonitorParam)
{
	return ((CChatServer *)MonitorParam)->MonitorThread_Chat_Update();
}

int							CChatServer::MonitorThread_Chat_Update()
{
	timeBeginPeriod(1);
	CLIENT *pClient = nullptr;
	SESSION *pSession = nullptr;
	char chkey = 0;

	while (1)
	{
		////////////////////////////////////////////////////////////////////////
		// TPS 업데이트
		////////////////////////////////////////////////////////////////////////
		_lUpdateMessagePoolTPS = _MessageMemoryPool.GetAllocCount();
		_lUpdateMessageQueueTPS = _lUpdateMessageQueueCounter;

		_lPlayerPoolTPS = _ClientMemoryPool.GetAllocCount();

		_lUpdateTPS = _lUpdateCounter;

		////////////////////////////////////////////////////////////////////////
		// 카운터 0 초기화
		////////////////////////////////////////////////////////////////////////
		_lUpdateMessageQueueCounter = 0;

		_lUpdateCounter = 0;
	
		_lClientCount = _Client.size();

		for (ClientIter iter = _Client.begin(); iter != _Client.end(); iter++)
		{
			pClient = iter->second;
			pSession = _Session[GET_SESSIONINDEX(pClient->iSessionID)];

			if (((GetTickCount64() - pClient->dwRecvPacketTime) > CConfigData::m_System_Timeout_Time) && (pClient->iAccountNo != 0))
			{
				LOG(L"ChatServer_Debug",
					LOG::LEVEL_DEBUG,
					L"TimeOut [SessionID : %p][iAccountNo : %d][Login : %d][X : %d][Y : %d][Disconnect : %d][RecvPacketTime : %d]",
					pClient->iSessionID, pClient->iAccountNo, pClient->bLogin, pClient->shSectorX, pClient->shSectorY, pClient->bDisconnect, GetTickCount64() - pClient->dwRecvPacketTime);
				LOG(L"ChatServer_Debug",
					LOG::LEVEL_DEBUG,
					L"SessionInfo [SendQSize : %d][IOCount : %d][ReleaseFlag : %d][SendFlag : %d][lSentPacket : %d]",
					pSession->_SendQ.GetUseSize(), pSession->_IOBlock->_iIOCount, pSession->_IOBlock->_iReleaseFlag, pSession->_bSendFlag, pSession->_lSentPacketCnt);
			}
				//Disconnect(pClient->iSessionID);
		}

		if (_kbhit() != 0){
			chkey = _getch();

			switch (chkey)
			{
			case 'w':
			case 'W':
				CCrashDump::Crash();
			}
		}

		Sleep(999);
	}

	timeEndPeriod(1);

	return 0;
}