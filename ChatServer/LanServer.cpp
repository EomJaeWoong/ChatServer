#include "stdafx.h"

//-----------------------------------------------------------------------------------------
// 생성자, 소멸자
//-----------------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	///////////////////////////////////////////////////////////////////////////////////////
	// 덤프 초기화
	///////////////////////////////////////////////////////////////////////////////////////
	CCrashDump::CCrashDump();

	///////////////////////////////////////////////////////////////////////////////////////
	// 프로파일러 초기화
	///////////////////////////////////////////////////////////////////////////////////////
	ProfileInit();

	SYSLOG_DIRECTORY(L"LanServerLog");
	SYSLOG_LEVEL(LOG::LEVEL_DEBUG);

	///////////////////////////////////////////////////////////////////////////////////////
	// 네트워크 패킷 변수 체크
	///////////////////////////////////////////////////////////////////////////////////////
	if (!CNPacket::_ValueSizeCheck())
		CCrashDump::Crash();

	///////////////////////////////////////////////////////////////////////////////////////
	// 빈 세션 인덱스 스택 생성
	///////////////////////////////////////////////////////////////////////////////////////
	_pBlankStack = new CLockfreeStack<int>();

	///////////////////////////////////////////////////////////////////////////////
	// 빈 세션 생성
	///////////////////////////////////////////////////////////////////////////////
	for (int iCnt = eMAX_SESSION - 1; iCnt >= 0; iCnt--)
	{
		_Session[iCnt] = new SESSION;
		///////////////////////////////////////////////////////////////////////////
		// 세션 정보 구조체 초기화
		///////////////////////////////////////////////////////////////////////////
		_Session[iCnt]->_SessionInfo._Socket = INVALID_SOCKET;
		memset(&_Session[iCnt]->_SessionInfo._wIP, 0, sizeof(_Session[iCnt]->_SessionInfo._wIP));
		_Session[iCnt]->_SessionInfo._iPort = 0;

		///////////////////////////////////////////////////////////////////////////
		// 세션 초기화
		///////////////////////////////////////////////////////////////////////////
		_Session[iCnt]->_iSessionID = -1;

		memset(&_Session[iCnt]->_SendOverlapped, 0, sizeof(OVERLAPPED));
		memset(&_Session[iCnt]->_RecvOverlapped, 0, sizeof(OVERLAPPED));

		_Session[iCnt]->_RecvQ.ClearBuffer();
		_Session[iCnt]->_SendQ.ClearBuffer();

		_Session[iCnt]->_bSendFlag = false;

		_Session[iCnt]->_IOBlock = (IOBlock *)_aligned_malloc(sizeof(IOBlock), 16);

		_Session[iCnt]->_IOBlock->_iIOCount = 0;
		_Session[iCnt]->_IOBlock->_iReleaseFlag = false;

		memset(_Session[iCnt]->_pSentPacket, 0, sizeof(_Session[iCnt]->_pSentPacket));
		_Session[iCnt]->_lSentPacketCnt = 0;

		InsertBlankSessionIndex(iCnt);
	}


	///////////////////////////////////////////////////////////////////////////////
	// LanServer 변수 설정
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;

	///////////////////////////////////////////////////////////////////////////////////////
	// 윈속 초기화
	///////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
		return;


	///////////////////////////////////////////////////////////////////////////////////////
	// 모니터링 스레드
	///////////////////////////////////////////////////////////////////////////////////////
	DWORD dwThreadID;
	_hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);
}

CLanServer::~CLanServer()
{
	CloseHandle(_hIOCP);

	for (int iCnt = 0; iCnt < eMAX_SESSION; iCnt++)
		delete _Session[iCnt];

	delete _pBlankStack;
}


//-----------------------------------------------------------------------------------------
// 서버 시작
//-----------------------------------------------------------------------------------------
bool				CLanServer::Start(WCHAR* wOpenIP, int iPort, int iWorkerThreadNum, bool bNagle, int iMaxConnect)
{
	int				result;

	///////////////////////////////////////////////////////////////////////////////////////
	// 리슨소켓 생성
	///////////////////////////////////////////////////////////////////////////////////////
	_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == _ListenSocket)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// bind
	///////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	result = bind(_ListenSocket, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (SOCKET_ERROR == result)
		return false;

	///////////////////////////////////////////////////////////////////////////////////
	// 소켓 Send Buffer 공간을 0으로 설정
	///////////////////////////////////////////////////////////////////////////////////
	int optval;
	setsockopt(_ListenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, 0);

	///////////////////////////////////////////////////////////////////////////////////////
	// listen
	///////////////////////////////////////////////////////////////////////////////////////
	result = listen(_ListenSocket, SOMAXCONN);
	if (SOCKET_ERROR == result)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// nagle option
	///////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;

	///////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port 생성
	///////////////////////////////////////////////////////////////////////////////////////
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == _hIOCP)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// Thread 생성
	///////////////////////////////////////////////////////////////////////////////////////
	DWORD dwThreadID;

	_hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	_hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	if (iWorkerThreadNum > eMAX_THREAD)
		return false;

	_iWorkerThreadNum = iWorkerThreadNum;

	for (int iCnt = 0; iCnt < iWorkerThreadNum; iCnt++)
	{
		_hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// 서버 멈춤
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::Stop()
{
	///////////////////////////////////////////////////////////////////////////////////////
	// 리슨 소켓 닫기
	///////////////////////////////////////////////////////////////////////////////////////
	closesocket(_ListenSocket);

	///////////////////////////////////////////////////////////////////////////////////////
	// accpet Thread가 종료되면
	// 모든 세션을 종료시키고 다른 쓰레드도 종료한다
	///////////////////////////////////////////////////////////////////////////////////////
	if (WAIT_OBJECT_0 == WaitForSingleObject(_hAcceptThread, INFINITE))
	{
		CloseHandle(_hAcceptThread);
		for (int iCnt = 0; iCnt < eMAX_SESSION; iCnt++)
			DisconnectSession(_Session[iCnt]);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	// session이 0이 될때까지 기다림
	///////////////////////////////////////////////////////////////////////////////////////
	while (GetSessionCount());

	///////////////////////////////////////////////////////////////////////////////////////
	// Worker Thread들에게 종료 status 날림
	///////////////////////////////////////////////////////////////////////////////////////
	PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);

	CloseHandle(_hIOCP);
	for (int iCnt = 0; iCnt < _iWorkerThreadNum; iCnt++)
		CloseHandle(_hWorkerThread[iCnt]);
}


///////////////////////////////////////////////////////////////////////////////////////////
// 패킷 보내기
///////////////////////////////////////////////////////////////////////////////////////////
bool				CLanServer::SendPacket(__int64 iSessionID, CNPacket *pPacket)
{
	SESSION *pSession = SessionGetLock(iSessionID);
	if (nullptr == pSession)
	{
		LOG(L"LanServer", LOG::LEVEL_DEBUG, L"Session Lost[Send Packet] : SessionID[%d]", iSessionID);
		return false;
	}

	if (iSessionID == pSession->_iSessionID)
	{
		pPacket->SetCustomShortHeader(pPacket->GetDataSize());

		PRO_BEGIN(L"Packet addref");
		pPacket->addRef();
		PRO_END(L"Packet addref");

		PRO_BEGIN(L"PacketQueue Put");
		pSession->_SendQ.Put(pPacket);
		PRO_END(L"PacketQueue Put");

		PRO_BEGIN(L"SendPost");
		SendPost(pSession);
		PRO_END(L"SendPost");

		InterlockedIncrement((LONG *)&_lSendPacketCounter);
	}

	SessionGetUnlock(pSession);

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// 연결 끊기
///////////////////////////////////////////////////////////////////////////////////////////
bool				CLanServer::Disconnect(__int64 iSessionID)
{
	SESSION *pSession = SessionGetLock(iSessionID);
	if (nullptr == pSession)
		return false;

	if (iSessionID == pSession->_iSessionID)
	{
		DisconnectSession(pSession);
	}

	SessionGetUnlock(pSession);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
// 실제 동작하는 스레드 부분
///////////////////////////////////////////////////////////////////////////////////////////
int					CLanServer::AccpetThread_update()
{
	HANDLE			hResult;
	int				iErrorCode;

	SOCKET			ClientSocket;
	SOCKADDR_IN		ClientAddr;
	int				iAddrLen = sizeof(SOCKADDR_IN);


	SESSIONINFO		SessionInfo;

	int				iBlankIndex;


	while (1)
	{
		///////////////////////////////////////////////////////////////////////////////////
		// accpet
		///////////////////////////////////////////////////////////////////////////////////
		ClientSocket = accept(_ListenSocket, (SOCKADDR *)&ClientAddr, &iAddrLen);
		if (INVALID_SOCKET == ClientSocket)
		{
			iErrorCode = WSAGetLastError();
			if ((WSAENOTSOCK == iErrorCode) ||
				(WSAEINTR == iErrorCode))
				break;

			CCrashDump::Crash();
		}


		InterlockedIncrement((LONG *)&_lAcceptCounter);
		InterlockedIncrement((LONG *)&_lAcceptTotalTPS);

		///////////////////////////////////////////////////////////////////////////////////
		// 세션 접속 정보 생성
		///////////////////////////////////////////////////////////////////////////////////
		SessionInfo._Socket = ClientSocket;
		InetNtop(AF_INET, &ClientAddr.sin_addr, SessionInfo._wIP, 16);
		SessionInfo._iPort = ntohs(ClientAddr.sin_port);

		///////////////////////////////////////////////////////////////////////////////////
		// 접속 요청(White IP만 접속하게 하기 등)
		///////////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(&SessionInfo))
			continue;

		iBlankIndex = GetBlankSessionIndex();

		///////////////////////////////////////////////////////////////////////////////////
		// 최대 세션 초과시
		///////////////////////////////////////////////////////////////////////////////////
		if (iBlankIndex < 0)
		{
			closesocket(ClientSocket);
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// keepalive 옵션
		//
		// onoff			 -> keepalive 동작 여부(0이 아니면 동작)
		// keepalivetime	 -> 첫 keepalive 패킷이 가기 전까지의 시간
		// keepaliveinterval -> 응답이 없을경우 연속 keepalive 패킷이 전송되는 간격 
		///////////////////////////////////////////////////////////////////////////////////
		tcp_keepalive tcpkl;

		tcpkl.onoff = 1;
		tcpkl.keepalivetime = 30000;
		tcpkl.keepaliveinterval = 2000;

		DWORD dwReturnByte;
		WSAIoctl(SessionInfo._Socket, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive),
			0, 0, &dwReturnByte, NULL, NULL);

		///////////////////////////////////////////////////////////////////////////////////
		// nagle 옵션
		///////////////////////////////////////////////////////////////////////////////////
		if (_bNagle)
		{
			char opt_val = true;
			setsockopt(SessionInfo._Socket, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof(opt_val));
		}

		///////////////////////////////////////////////////////////////////////////////////
		// 빈 세션 찾아 세션 생성
		///////////////////////////////////////////////////////////////////////////////////
		_Session[iBlankIndex]->_SessionInfo = SessionInfo;

		///////////////////////////////////////////////////////////////////////////////////
		// 세션 ID 조합해서 만들기
		///////////////////////////////////////////////////////////////////////////////////
		int iSessionID = InterlockedIncrement((LONG *)&_iSessionID);
		_Session[iBlankIndex]->_iSessionID = COMBINE_ID_WITH_INDEX(iSessionID, iBlankIndex);

		/////////////////////////////////////////////////////////////////////
		// IOCP 등록
		/////////////////////////////////////////////////////////////////////
		hResult = CreateIoCompletionPort((HANDLE)_Session[iBlankIndex]->_SessionInfo._Socket,
			_hIOCP,
			(ULONG_PTR)_Session[iBlankIndex],
			0);
		if (!hResult)
			PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);

		/////////////////////////////////////////////////////////////////////
		// OnClientJoin
		// 컨텐츠쪽에 세션이 들어왔음을 알림
		// 로그인 패킷 보내는 중에 끊길 수 있으니 IOCount를 미리 올려준다
		/////////////////////////////////////////////////////////////////////
		InterlockedIncrement64((LONG64 *)&_Session[iBlankIndex]->_IOBlock->_iIOCount);
		InterlockedExchange64((LONG64 *)&_Session[iBlankIndex]->_IOBlock->_iReleaseFlag, FALSE);

		OnClientJoin(&_Session[iBlankIndex]->_SessionInfo, _Session[iBlankIndex]->_iSessionID);

		InterlockedIncrement((long *)&_lSessionCount);

		PRO_BEGIN(L"RecvPost - AccpetTH");
		RecvPost(_Session[iBlankIndex], true);
		PRO_END(L"RecvPost - AccpetTH");
	}

	return 0;
}

int					CLanServer::WorkerThread_update()
{
	int				result;

	OVERLAPPED		*pOverlapped;
	SESSION			*pSession;
	DWORD			dwTransferred;

	while (1)
	{
		pOverlapped = NULL;
		pSession = NULL;
		dwTransferred = 0;

		PRO_BEGIN(L"GQCS IOComplete");
		result = GetQueuedCompletionStatus(
			_hIOCP,
			&dwTransferred,
			(PULONG_PTR)&pSession,
			&pOverlapped,
			INFINITE);
		PRO_END(L"GQCS IOComplete");

		OnWorkerThreadBegin();

		///////////////////////////////////////////////////////////////////////////////////
		// Error, 종료 처리
		///////////////////////////////////////////////////////////////////////////////////
		// IOCP 에러 서버 종료
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// 워커스레드 정상 종료
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// 정상종료
		// 클라이언트 에서 closesocket() 혹은 shutdown() 함수를 호출한 시점
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			Disconnect(pSession->_iSessionID);
		}
		//----------------------------------------------------------------------------

		if (pOverlapped == &(pSession->_RecvOverlapped))
		{
			PRO_BEGIN(L"CompleteRecv");
			CompleteRecv(pSession, dwTransferred);
			PRO_END(L"CompleteRecv");
		}

		if (pOverlapped == &(pSession->_SendOverlapped))
		{
			PRO_BEGIN(L"CompleteSend");
			CompleteSend(pSession, dwTransferred);
			PRO_END(L"CompleteSend");
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
			ReleaseSession(pSession);

		OnWorkerThreadEnd();
	}

	return true;
}

int					CLanServer::MonitorThread_update()
{
	timeBeginPeriod(1);

	while (1)
	{
		_lAcceptTPS = _lAcceptCounter;
		_lRecvPacketTPS = _lRecvPacketCounter;
		_lSendPacketTPS = _lSendPacketCounter;
		_lPacketPoolTPS = CNPacket::GetPacketCount();

		_lAcceptCounter = 0;
		_lRecvPacketCounter = 0;
		_lSendPacketCounter = 0;

		Sleep(999);
	}

	timeEndPeriod(1);

	return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////
// Recv, Send 등록
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int result, iCount = 1;
	DWORD dwRecvSize, dwFlag = 0;
	WSABUF wBuf[2];

	///////////////////////////////////////////////////////////////////////////////////////
	// WSABUF 등록
	///////////////////////////////////////////////////////////////////////////////////////
	wBuf[0].buf = pSession->_RecvQ.GetWriteBufferPtr();
	wBuf[0].len = pSession->_RecvQ.GetNotBrokenPutSize();

	///////////////////////////////////////////////////////////////////////////////////////
	// 공간이 남아있을 경우 남은 공간 등록
	///////////////////////////////////////////////////////////////////////////////////////
	if (pSession->_RecvQ.GetFreeSize() > pSession->_RecvQ.GetNotBrokenPutSize())
	{
		wBuf[1].buf = pSession->_RecvQ.GetBufferPtr();
		wBuf[1].len = pSession->_RecvQ.GetFreeSize() -
			pSession->_RecvQ.GetNotBrokenPutSize();

		iCount++;
	}

	memset(&pSession->_RecvOverlapped, 0, sizeof(OVERLAPPED));

	///////////////////////////////////////////////////////////////////////////////////////
	// 첫 접속시의 Recv는 IOCount를 올리지 않음(로그인 패킷 때문)
	///////////////////////////////////////////////////////////////////////////////////////
	if (!bAcceptRecv)
		InterlockedIncrement64((LONG64 *)&pSession->_IOBlock->_iIOCount);

	PRO_BEGIN(L"WSARecv");
	result = WSARecv(
		pSession->_SessionInfo._Socket,
		wBuf,
		iCount,
		&dwRecvSize,
		&dwFlag,
		&pSession->_RecvOverlapped,
		NULL
		);

	if (result == SOCKET_ERROR)
	{
		int iErrorCode = GetLastError();
		///////////////////////////////////////////////////////////////////////////////////
		// WSA_IO_PENDING -> Overlapped 연산이 준비되었으나 완료되지 않은 경우
		// 이 외에는 에러로 봄
		///////////////////////////////////////////////////////////////////////////////////
		if (iErrorCode != WSA_IO_PENDING)
		{
			///////////////////////////////////////////////////////////////////////////////
			// 10054 : 클라이언트 쪽에서 강제로 끊어진 경우
			// 10058 : 해당 소켓이 shutdown된 경우
			///////////////////////////////////////////////////////////////////////////////
			if ((WSAECONNABORTED != iErrorCode) &&
				(WSAECONNRESET != iErrorCode) &&
				(WSAESHUTDOWN != iErrorCode))
				CCrashDump::Crash();


			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
				ReleaseSession(pSession);
		}
	}
	PRO_END(L"WSARecv");
}

bool				CLanServer::SendPost(SESSION *pSession)
{
	int result, iCount;
	DWORD dwSendSize, dwFlag = 0;
	WSABUF wBuf[eMAX_WSABUF];

	do
	{
		///////////////////////////////////////////////////////////////////////////////////
		// SendFlag 확인 및 변경
		///////////////////////////////////////////////////////////////////////////////////
		bool bSendFlag = InterlockedCompareExchange((long *)&pSession->_bSendFlag, true, false);
		if (true == bSendFlag)
			break;

		///////////////////////////////////////////////////////////////////////////////////
		// SendQ 사이즈 측정
		///////////////////////////////////////////////////////////////////////////////////
		int iSendQUseSize = pSession->_SendQ.GetUseSize();

		///////////////////////////////////////////////////////////////////////////////////
		// SendQ사이즈 다시 확인
		///////////////////////////////////////////////////////////////////////////////////
		if (0 == iSendQUseSize)
		{
			InterlockedExchange((long *)&pSession->_bSendFlag, false);

			///////////////////////////////////////////////////////////////////////////////
			// SendQ에 사이즈가 있을떄(전에 측정과 다를 때)
			///////////////////////////////////////////////////////////////////////////////
			if (!pSession->_SendQ.isEmpty())
				continue;

			break;
		}

		if (eMAX_WSABUF <= iSendQUseSize)
			iSendQUseSize = eMAX_WSABUF;


		///////////////////////////////////////////////////////////////////////////////////
		// WSABUF에 패킷 넣기
		///////////////////////////////////////////////////////////////////////////////////
		CNPacket *pPacket = nullptr;
		for (iCount = 0; iCount < iSendQUseSize; iCount++)
		{
			if (!pSession->_SendQ.Get(&pPacket))
				break;

			wBuf[iCount].buf = (char *)pPacket->GetBufferHeaderPtr();
			wBuf[iCount].len = pPacket->GetDataSize();

			pSession->_pSentPacket[iCount] = (char *)pPacket;
		}

		pSession->_lSentPacketCnt += iCount;

		memset(&pSession->_SendOverlapped, 0, sizeof(OVERLAPPED));

		InterlockedIncrement64((LONG64 *)&pSession->_IOBlock->_iIOCount);

		PRO_BEGIN(L"WSASend");
		result = WSASend(
			pSession->_SessionInfo._Socket,
			wBuf,
			iCount,
			&dwSendSize,
			dwFlag,
			&pSession->_SendOverlapped,
			NULL
			);

		if (result == SOCKET_ERROR)
		{
			int iErrorCode = WSAGetLastError();
			///////////////////////////////////////////////////////////////////////////////////
			// WSA_IO_PENDING -> Overlapped 연산이 준비되었으나 완료되지 않은 경우
			// 이 외에는 에러로 봄
			///////////////////////////////////////////////////////////////////////////////////
			if (iErrorCode != WSA_IO_PENDING)
			{
				// 시스템 로그 남기기
				if (WSAENOBUFS == iErrorCode)
					CCrashDump::Crash();

				///////////////////////////////////////////////////////////////////////////////
				// 클라이언트 쪽에서 강제로 끊어진 경우
				///////////////////////////////////////////////////////////////////////////////
				if ((WSAECONNABORTED != iErrorCode) &&
					(WSAECONNRESET != iErrorCode) &&
					(WSAESHUTDOWN != iErrorCode))
					CCrashDump::Crash();

				if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
					ReleaseSession(pSession);
			}
		}
		PRO_END(L"WSASend");
	} while (0);

	return true;
}



///////////////////////////////////////////////////////////////////////////////////////////
// Recv, Send 처리
///////////////////////////////////////////////////////////////////////////////////////////
bool				CLanServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	short header;
	CNPacket *pPacket = nullptr;

	//////////////////////////////////////////////////////////////////////////////
	// RecvQ WritePos 이동(받은 만큼)
	//////////////////////////////////////////////////////////////////////////////
	if (dwTransferred != pSession->_RecvQ.MoveWritePos(dwTransferred))
		CCrashDump::Crash();

	while (pSession->_RecvQ.GetUseSize() > 0)
	{
		PRO_BEGIN(L"Packet Alloc");
		pPacket = CNPacket::Alloc();
		PRO_END(L"Packet Alloc");

		PRO_BEGIN(L"Recv BufferDeque");
		//////////////////////////////////////////////////////////////////////////
		// RecvQ에 헤더 길이만큼 있는지 검사 후 있으면 Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() <= sizeof(header))
			break;
		pSession->_RecvQ.Peek((char *)&header, sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// RecvQ에 헤더 길이 + Payload 만큼 있는지 검사 후 헤더 제거
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() < sizeof(header) + header)
			break;;
		pSession->_RecvQ.RemoveData(sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// Payload를 뽑은 후 패킷 클래스에 넣음
		//////////////////////////////////////////////////////////////////////////
		pPacket->PutData((unsigned char *)pSession->_RecvQ.GetReadBufferPtr(), header);
		pSession->_RecvQ.RemoveData(header);
		PRO_END(L"Recv BufferDeque");

		//////////////////////////////////////////////////////////////////////////
		// OnRecv 호출
		//////////////////////////////////////////////////////////////////////////
		PRO_BEGIN(L"OnRecv");
		OnRecv(pSession->_iSessionID, pPacket);
		PRO_END(L"OnRecv");

		InterlockedIncrement((LONG *)&_lRecvPacketCounter);
	}

	PRO_BEGIN(L"RecvPost");
	RecvPost(pSession);
	PRO_END(L"RecvPost");

	return true;
}

bool				CLanServer::CompleteSend(SESSION *pSession, DWORD dwTransferred)
{
	int			iSentCnt;

	//////////////////////////////////////////////////////////////////////////////
	// 보내기 완료된 데이터 제거
	//////////////////////////////////////////////////////////////////////////////
	PRO_BEGIN(L"SentPacket Remove");
	for (iSentCnt = 0; iSentCnt < pSession->_lSentPacketCnt; iSentCnt++)
		((CNPacket *)pSession->_pSentPacket[iSentCnt])->Free();

	pSession->_lSentPacketCnt -= iSentCnt;

	PRO_END(L"SentPacket Remove");

	//////////////////////////////////////////////////////////////////////////////
	// 다 보냈다고 Flag 변환
	//////////////////////////////////////////////////////////////////////////////
	InterlockedExchange((long *)&pSession->_bSendFlag, false);

	PRO_BEGIN(L"SendPost - WorkerTh");
	//////////////////////////////////////////////////////////////////////////////
	// 보낼게 남아있으면 다시 등록
	//////////////////////////////////////////////////////////////////////////////
	if (!pSession->_SendQ.isEmpty())
		SendPost(pSession);

	PRO_END(L"SendPost - WorkerTh");

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// 세션 동기화		 ->		Disconnect, SendPacket
///////////////////////////////////////////////////////////////////////////////////////////
SESSION*			CLanServer::SessionGetLock(__int64 iSessionID)
{
	int iSessionIndex = GET_SESSIONINDEX(iSessionID);

	///////////////////////////////////////////////////////////////////////////////////////
	// 릴리즈 되지 않게 확인
	///////////////////////////////////////////////////////////////////////////////////////
	if (1 == InterlockedIncrement64((LONG64 *)&_Session[iSessionIndex]->_IOBlock->_iIOCount))
	{
		if (0 == InterlockedDecrement64((LONG64 *)&_Session[iSessionIndex]->_IOBlock->_iIOCount))
			ReleaseSession(_Session[iSessionIndex]);

		return nullptr;
	}

	return _Session[iSessionIndex];
}

void				CLanServer::SessionGetUnlock(SESSION *pSession)
{
	///////////////////////////////////////////////////////////////////////////////////////
	// 다시 작업 카운트 낮춰줌
	///////////////////////////////////////////////////////////////////////////////////////
	if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
		ReleaseSession(pSession);
}



///////////////////////////////////////////////////////////////////////////////////////////
// 빈 세션 얻기
///////////////////////////////////////////////////////////////////////////////////////////
int					CLanServer::GetBlankSessionIndex()
{
	int iBlankIndex;

	if (_pBlankStack->isEmpty())
		iBlankIndex = -1;
	else
		_pBlankStack->Pop(&iBlankIndex);

	return iBlankIndex;
}

///////////////////////////////////////////////////////////////////////////////////////////
// 세션 반납
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::InsertBlankSessionIndex(int iSessionIndex)
{
	_pBlankStack->Push(iSessionIndex);
}


///////////////////////////////////////////////////////////////////////////////////////////
// Disconnection
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::DisconnectSession(SESSION *pSession)
{
	CloseSocket(pSession->_SessionInfo._Socket);
}

///////////////////////////////////////////////////////////////////////////////////////////
// 소켓 연결 끊기
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::CloseSocket(SOCKET socket)
{
	shutdown(socket, SD_BOTH);
}

///////////////////////////////////////////////////////////////////////////////////////////
// Release
///////////////////////////////////////////////////////////////////////////////////////////
void				CLanServer::ReleaseSession(SESSION *pSession)
{
	IOBlock stCompareBlock;

	stCompareBlock._iIOCount = 0;
	stCompareBlock._iReleaseFlag = 0;

	///////////////////////////////////////////////////////////////////////////////////////
	// 진짜 릴리즈 해야하는 경우인지 검사
	///////////////////////////////////////////////////////////////////////////////////////
	if (!InterlockedCompareExchange128(
		(LONG64 *)pSession->_IOBlock,
		(LONG64)true,
		(LONG64)0,
		(LONG64 *)&stCompareBlock
		))
		return;

	closesocket(pSession->_SessionInfo._Socket);

	pSession->_SessionInfo._Socket = INVALID_SOCKET;

	pSession->_SessionInfo._iPort = 0;
	memset(&pSession->_SessionInfo._wIP, 0, sizeof(pSession->_SessionInfo._wIP));

	pSession->_RecvQ.ClearBuffer();

	///////////////////////////////////////////////////////////////////////////////////////
	// Packet Send Queue(패킷도 다 비우고 큐를 비워야함
	///////////////////////////////////////////////////////////////////////////////////////
	while (!pSession->_SendQ.isEmpty())
	{
		CNPacket *pFreePacket;
		pSession->_SendQ.Get(&pFreePacket);
		pFreePacket->Free();
	}

	memset(&pSession->_RecvOverlapped, 0, sizeof(OVERLAPPED));
	memset(&pSession->_SendOverlapped, 0, sizeof(OVERLAPPED));

	for (int iSentCnt = 0; iSentCnt < pSession->_lSentPacketCnt; iSentCnt++)
	{
		CNPacket *pPacket = (CNPacket *)pSession->_pSentPacket[iSentCnt];
		pPacket->Free();
		pSession->_pSentPacket[iSentCnt] = nullptr;
	}

	pSession->_lSentPacketCnt = 0;

	InterlockedExchange((long *)&pSession->_bSendFlag, false);

	OnClientLeave(pSession->_iSessionID);

	InsertBlankSessionIndex(GET_SESSIONINDEX(pSession->_iSessionID));

	InterlockedDecrement((LONG *)&_lSessionCount);
}



///////////////////////////////////////////////////////////////////////////////////////////
// 실제 스레드 부분
///////////////////////////////////////////////////////////////////////////////////////////
unsigned __stdcall	CLanServer::AcceptThread(LPVOID AcceptParam)
{
	return ((CLanServer *)AcceptParam)->AccpetThread_update();
}

unsigned __stdcall	CLanServer::WorkerThread(LPVOID WorkerParam)
{
	return ((CLanServer *)WorkerParam)->WorkerThread_update();
}

unsigned __stdcall	CLanServer::MonitorThread(LPVOID MonitorParam)
{
	return ((CLanServer *)MonitorParam)->MonitorThread_update();
}
