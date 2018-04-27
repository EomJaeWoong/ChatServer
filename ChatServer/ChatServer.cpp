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

void						CChatServer::OnRecv(__int64 iSessionID, CNPacket* pPacket)
{
	MESSAGE *pMessage = MakeMessage_Packet(iSessionID, pPacket);
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
	int			iResult;
	MESSAGE		*pMessage = nullptr;

	while (!_bShutdown)
	{
		iResult = WaitForSingleObject(_hUpdateEvent, INFINITE);
		if (WAIT_OBJECT_0 != iResult)
			CCrashDump::Crash();

		while (!_MessageQueue.isEmpty())
		{
			if (!_MessageQueue.Get(&pMessage))
				break;
		
			if (!CompleteMessage(pMessage))
				CCrashDump::Crash();

			InterlockedIncrement(&_lUpdateCounter);

			_MessageMemoryPool.Free(pMessage);
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
	/////////////////////////////////////////////////////////////////////////////
	// e_MESSAGE_NEW_CONNECTION	- 신규 접속
	// e_MESSAGE_DISCONNECTION	- 접속 해제
	// e_MESSAGE_PACKET			- 패킷 처리
	/////////////////////////////////////////////////////////////////////////////
	switch (pMessage->wType)
	{
	case e_MESSAGE_NEW_CONNECTION:
		return CreateClient(pMessage->iSessionID);
		break;

	case e_MESSAGE_DISCONNECTION:
		return DeleteClient(pMessage->iSessionID);
		break;

	case e_MESSAGE_PACKET:
		return CompletePacket(pMessage->iSessionID, pMessage->pPacket);
		break;

	default:
		Disconnect(pMessage->iSessionID);
		break;

	}

	return false;
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
bool						CChatServer::CompletePacket(__int64 iSessionID, CNPacket *pPacket)
{
	WORD wType = 0xffff;

	*pPacket >> wType;

	/////////////////////////////////////////////////////////////////////////////
	// 패킷 받은 시간 업데이트
	/////////////////////////////////////////////////////////////////////////////
	CLIENT *pClient = SearchClient(iSessionID);
	if (nullptr == pClient)
		return false;

	pClient->dwRecvPacketTime = GetTickCount64();


	/////////////////////////////////////////////////////////////////////////////
	// en_PACKET_CS_CHAT_REQ_LOGIN			- 로그인 요청
	// en_PACKET_CS_CHAT_REQ_SECTOR_MOVE	- 섹터 이동 요청
	// en_PACKET_CS_CHAT_REQ_MESSAGE		- 메시지 요청
	// en_PACKET_CS_CHAT_REQ_HEARTBEAT		- 하트비트
	/////////////////////////////////////////////////////////////////////////////
	switch (wType)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN :
		return PacketProc_ReqLogin(iSessionID, pPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE :
		return PacketProc_ReqSectorMove(iSessionID, pPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_MESSAGE :
		return PacketProc_ReqMessage(iSessionID, pPacket);
		break;

	case en_PACKET_CS_CHAT_REQ_HEARTBEAT :
		return PacketProc_ReqHeartbeat(iSessionID, pPacket);
		break;

	default :
		Disconnect(iSessionID);
		break;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// 패킷 처리
////////////////////////////////////////////////////////////////////////////////
bool						CChatServer::PacketProc_ReqLogin(__int64 iSessionID, CNPacket *pPacket)
{
	CNPacket	*pSendPacket;
	CLIENT		*pClient = SearchClient(iSessionID);
	
	__int64		iAccountNo;
	BYTE		byStatus = 1;

	////////////////////////////////////////////////////////////////////////////
	// 클라이언트 찾기
	////////////////////////////////////////////////////////////////////////////
	if (nullptr == pClient)
	{
		pClient->bDisconnect = true;
		Disconnect(iSessionID);
		byStatus = 0;
	}

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////	
	*pPacket >> iAccountNo;
	/*
	ClientIter iter;
	for (iter = _Client.begin(); iter != _Client.end(); iter++)
	{
		if (iter->second->iAccountNo == iAccountNo)
		{
			Disconnect(iter->first);
			if (CreateClient(iSessionID))
				pClient = (_Client.find(iSessionID))->second;
		}
	}
	*/

	pClient->iAccountNo = iAccountNo;

	////////////////////////////////////////////////////////////////////////////
	// ID, Nickname 확인
	////////////////////////////////////////////////////////////////////////////
	pPacket->GetData((unsigned char *)pClient->szID, sizeof(pClient->szID));
	pPacket->GetData((unsigned char *)pClient->szNickname, sizeof(pClient->szNickname));
	

	// 세션키 확인해야 하는데 지금은 그냥 믿고 감
	pPacket->GetData((unsigned char *)pClient->chSessionKey, 64);

	pPacket->Free();

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResLogin(pClient->iAccountNo, byStatus);
	SendPacket_One(pClient, pSendPacket);

	pSendPacket->Free();

	InterlockedIncrement(&_lPlayerCount);

	return true;
}

bool						CChatServer::PacketProc_ReqSectorMove(__int64 iSessionID, CNPacket *pPacket)
{
	CNPacket	*pSendPacket;
	CLIENT		*pClient = SearchClient(iSessionID);

	__int64		iAccountNo;
	WORD		wSectorX = -1, wSectorY = -1;

	////////////////////////////////////////////////////////////////////////////
	// 클라이언트 찾기
	////////////////////////////////////////////////////////////////////////////
	if (nullptr == pClient)
		return false;
		

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////
	*pPacket >> iAccountNo;
	if (iAccountNo != pClient->iAccountNo)
	{
		pClient->bDisconnect = true;
		Disconnect(iSessionID);
		return false;
	}

	////////////////////////////////////////////////////////////////////////////
	// 섹터 뽑아서 검사후 넣고 섹터 업데이트
	////////////////////////////////////////////////////////////////////////////
	*pPacket >> wSectorX;
	*pPacket >> wSectorY;

	pPacket->Free();

	////////////////////////////////////////////////////////////////////////////
	// 섹터 업데이트
	////////////////////////////////////////////////////////////////////////////
	UpdateSector(pClient, wSectorX, wSectorY);

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResSectorMove(pClient->iAccountNo, pClient->shSectorX, pClient->shSectorY);
	SendPacket_One(pClient, pSendPacket);

	pSendPacket->Free();

	return true;
}

bool						CChatServer::PacketProc_ReqMessage(__int64 iSessionID, CNPacket *pPacket)
{
	CNPacket	*pSendPacket;
	CLIENT		*pClient = SearchClient(iSessionID);

	__int64		iAccountNo;
	WORD		wMessageLen = 0;
	WCHAR		*pMessage;

	////////////////////////////////////////////////////////////////////////////
	// 클라이언트 찾기
	////////////////////////////////////////////////////////////////////////////
	if (nullptr == pClient)
		return false;

	////////////////////////////////////////////////////////////////////////////
	// AccountNo 검사
	////////////////////////////////////////////////////////////////////////////
	*pPacket >> iAccountNo;
	if (iAccountNo != pClient->iAccountNo)
	{
		pClient->bDisconnect = true;
		Disconnect(iSessionID);
		return false;
	}

	if (pClient->shSectorX == -1)
		CCrashDump::Crash();

	////////////////////////////////////////////////////////////////////////////
	// 메시지 길이 뽑고 메시지 뽑기
	////////////////////////////////////////////////////////////////////////////
	*pPacket >> wMessageLen;
	
	pMessage = new WCHAR[wMessageLen / 2];
	memset(pMessage, 0, wMessageLen);
	pPacket->GetData((unsigned char *)pMessage, wMessageLen);
	

	////////////////////////////////////////////////////////////////////////////
	// 패킷 만들어 보내기
	////////////////////////////////////////////////////////////////////////////
	pSendPacket = MakePacket_ResMessage(pClient->iAccountNo, pClient->szID, pClient->szNickname, wMessageLen, pMessage);
	SendPacket_Around(pClient, pSendPacket, true);

	delete[] pMessage;
	pPacket->Free();
	pSendPacket->Free();

	return true;
}

bool						CChatServer::PacketProc_ReqHeartbeat(__int64 iSessionID, CNPacket *pPacket)
{
	CLIENT		*pClient = SearchClient(iSessionID);

	////////////////////////////////////////////////////////////////////////////
	// 클라이언트 찾기
	////////////////////////////////////////////////////////////////////////////
	if (nullptr == pClient)
		return false;

	pPacket->Free();

	if (GetTickCount64() - pClient->dwRecvPacketTime > CConfigData::m_System_Timeout_Time)
	{
		pClient->bDisconnect = FALSE;
		Disconnect(iSessionID);
	}

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

CNPacket*					CChatServer::MakePacket_ResSectorMove(__int64 iAccountNo, WORD wSectorX, WORD wSectorY)
{
	CNPacket *pPacket = CNPacket::Alloc();
	if (nullptr == pPacket)
		return nullptr;

	*pPacket << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE;

	*pPacket << iAccountNo;
	*pPacket << wSectorX;
	*pPacket << wSectorY;

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

			if (pSendClient->shSectorX == -1)
				CCrashDump::Crash();

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

	else if (iter == _Client.end())
		CCrashDump::Crash();

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
void						CChatServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND *pSectorAround)
{
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;

	for (int iCntY = 0; iCntY < 3; iCntY++)
	{
		if (iSectorY + iCntY < 0 || iSectorY + iCntY >= eSECTOR_MAX_Y)
			continue;

		for (int iCntX = 0; iCntX < 3; iCntX++)
		{
			if (iSectorX + iCntX < 0 || iSectorX + iCntX >= eSECTOR_MAX_X)
				continue;

			pSectorAround->Around[pSectorAround->iCount].iX = iSectorX + iCntX;
			pSectorAround->Around[pSectorAround->iCount].iY = iSectorY + iCntY;
			pSectorAround->iCount++;
		}
	}
}

void						CChatServer::UpdateSector(CLIENT *pClient, WORD wNewSectorX, WORD wNewSectorY)
{
	__int64 iSessionID = pClient->iSessionID;

	/////////////////////////////////////////////////////////////////////////////
	//Sector영역 벗어날 때
	/////////////////////////////////////////////////////////////////////////////
	if (wNewSectorX < 0 || wNewSectorX >= eSECTOR_MAX_X)		return;
	if (wNewSectorY < 0 || wNewSectorY >= eSECTOR_MAX_Y)		return;

	if ((pClient->shSectorX == wNewSectorX) && (pClient->shSectorY == wNewSectorY))
		return;

	/////////////////////////////////////////////////////////////////////////////
	// 원래 있던 곳에서 지우기
	/////////////////////////////////////////////////////////////////////////////
	if ((pClient->shSectorX != -1) && (pClient->shSectorY != -1))
		_Sector[pClient->shSectorY][pClient->shSectorX].remove(iSessionID);

	_Sector[wNewSectorY][wNewSectorX].push_back(iSessionID);

	pClient->shSectorX = wNewSectorX;
	pClient->shSectorY = wNewSectorY;
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

		Sleep(999);
	}

	timeEndPeriod(1);

	return 0;
}