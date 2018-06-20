#ifndef __CHATSERVER__H__
#define __CHATSERVER__H__

class CChatServer : public CNetServer
{
public :
	enum eChatServerConfig
	{
		///////////////////////////////
		// Message Type
		///////////////////////////////
		e_MESSAGE_NEW_CONNECTION = 0,
		e_MESSAGE_DISCONNECTION,
		e_MESSAGE_PACKET,

		///////////////////////////////
		// Config
		///////////////////////////////
		eMAX_ID			= 20,
		eMAX_NICKNAME	= 20,

		eSECTOR_MAX_Y	= 50,
		eSECTOR_MAX_X	= 50
	};

	/*-----------------------------------------------------------------------------*/
	// 메시지 구조체
	/*-----------------------------------------------------------------------------*/
	typedef struct st_MESSAGE
	{
		WORD			wType;

		__int64			iSessionID;
		CNPacket		*pPacket;

		LPVOID			_Debug;
	} MESSAGE;

	/*-----------------------------------------------------------------------------*/
	// Sector 하나의 좌표 정보
	/*-----------------------------------------------------------------------------*/
	typedef struct st_SECTOR_POS
	{
		short			iX;
		short			iY;

	} SECTORINFO;


	/*-----------------------------------------------------------------------------*/
	// Client 구조체
	/*-----------------------------------------------------------------------------*/
	typedef struct st_Client
	{
		__int64			iAccountNo;

		__int64			iSessionID;

		WCHAR			szID[20];
		WCHAR			szNickname[20];

		short			shSectorX;
		short			shSectorY;

		BYTE			chSessionKey[64];

		BOOL			bDisconnect;
		ULONGLONG		dwRecvPacketTime;
	} CLIENT;


	/*-----------------------------------------------------------------------------*/
	// 특정 위치 주변의 9개 섹터 정보
	/*-----------------------------------------------------------------------------*/
	struct st_SECTOR_AROUND
	{
		int				iCount;
		st_SECTOR_POS	Around[9];
	};

	/*-----------------------------------------------------------------------------*/
	// 타입 정의
	/*-----------------------------------------------------------------------------*/
	typedef list<__int64> SECTOR;
	typedef map<__int64, CLIENT *> CLIENTLIST;

	typedef list<__int64>::iterator SectorIter;
	typedef map<__int64, CLIENT *>::iterator ClientIter;


public :
	CChatServer();
	virtual ~CChatServer();

	bool						Start(WCHAR* wOpenIP, int iPort, int iWorkerThreadNum, bool bNagle, int iMaxConnect);
	void						Stop();


public :
	virtual void				OnClientJoin(SESSIONINFO *pSessionInfo, __int64 iSessionID);
	virtual void				OnClientLeave(__int64 iSessionID);
	virtual bool				OnConnectionRequest(SESSIONINFO *pSessionInfo);

	virtual void				OnRecv(__int64 iSessionID, CNPacket* pPacket);
	virtual void				OnSend(__int64 iSessionID, int iSendsize);

	virtual void				OnWorkerThreadBegin();
	virtual void				OnWorkerThreadEnd();

	virtual void				OnError(int iErrorCode, WCHAR *wErrorMsg);

private :
	/////////////////////////////////////////////////////////////////////////////////
	// Update Thread
	/////////////////////////////////////////////////////////////////////////////////
	static unsigned __stdcall	UpdateThread(LPVOID updateParam);
	int							UpdateThread_update();

	/////////////////////////////////////////////////////////////////////////////////
	// ChatServer 모니터링 스레드
	/////////////////////////////////////////////////////////////////////////////////
	static unsigned __stdcall	MonitorThread_Chat(LPVOID MonitorParam);
	int							MonitorThread_Chat_Update();
	
	/////////////////////////////////////////////////////////////////////////////////
	// 메시지 처리, 제작
	/////////////////////////////////////////////////////////////////////////////////
	bool						CompleteMessage(MESSAGE *pMessage);

	MESSAGE*					MakeMessage_NewConnection(__int64 iSessionID);
	MESSAGE*					MakeMessage_Disconnection(__int64 iSessionID);
	MESSAGE*					MakeMessage_Packet(__int64 iSessionID, CNPacket *pPacket);

	/////////////////////////////////////////////////////////////////////////////////
	// 패킷 처리, 제작
	/////////////////////////////////////////////////////////////////////////////////
	bool						CompletePacket(__int64 iSessionID, CNPacket *pPacket);

	bool						PacketProc_ReqLogin(__int64 iSessionID, CNPacket *pPacket);
	bool						PacketProc_ReqSectorMove(__int64 iSessionID, CNPacket *pPacket);
	bool						PacketProc_ReqMessage(__int64 iSessionID, CNPacket *pPacket);
	bool						PacketProc_ReqHeartbeat(__int64 iSessionID, CNPacket *pPacket);

	CNPacket*					MakePacket_ResLogin(__int64 iAccountNo, BYTE byStatus);
	CNPacket*					MakePacket_ResSectorMove(__int64 iAccountNo, WORD wSectorX, WORD wSectorY);
	CNPacket*					MakePacket_ResMessage(__int64 iAccountNo, WCHAR *szID, WCHAR *szNickname, WORD wMessageLen, WCHAR *pMessage);


	/////////////////////////////////////////////////////////////////////////////////
	// Send
	/////////////////////////////////////////////////////////////////////////////////
	void						SendPacket_One(CLIENT *pClient, CNPacket *pPacket);
	void						SendPacket_Around(CLIENT *pClient, CNPacket *pPacket, bool bSendMe = false);
	void						SendPacket_Broadcast(CNPacket *pPacket);


	/////////////////////////////////////////////////////////////////////////////////
	// 클라이언트 생성, 삭제, 찾기
	/////////////////////////////////////////////////////////////////////////////////
	bool						CreateClient(__int64 iSessionID);
	CLIENT*						SearchClient(__int64 iSessionID);
	bool						DeleteClient(__int64 iSessionID);

	/////////////////////////////////////////////////////////////////////////////////
	// 섹터 관련 함수
	// GetSectorAround		- 주변 섹터 얻기
	// UpdateSector			- 해당 클라이언트 섹터에 대해 업데이트(지우고 추가)
	/////////////////////////////////////////////////////////////////////////////////
	void						GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND *pSectorAround);
	void						UpdateSector(CLIENT *pClient, WORD wNewSectorX, WORD wNewSectorY);

public :
	/////////////////////////////////////////////////////////////////////////////////
	// 모니터링 항목들
	/////////////////////////////////////////////////////////////////////////////////
	long						_lUpdateMessageQueueCounter;

	long						_lUpdateCounter;

	long						_lUpdateMessagePoolTPS;
	long						_lUpdateMessageQueueTPS;

	long						_lPlayerPoolTPS;
	long						_lPlayerCount;

	long						_lUpdateTPS;



private :
	bool						_bShutdown;

	/////////////////////////////////////////////////////////////////////////////////
	// Update Thread용 Handle, Event
	/////////////////////////////////////////////////////////////////////////////////
	HANDLE						_hUpdateThread;
	HANDLE						_hUpdateEvent;

	/////////////////////////////////////////////////////////////////////////////////
	// ChatServer Monitor Thread
	/////////////////////////////////////////////////////////////////////////////////
	HANDLE						_hMonitorThreadChat;

	/////////////////////////////////////////////////////////////////////////////////
	// 패킷을 담는 메시지 큐
	/////////////////////////////////////////////////////////////////////////////////
	CLockfreeQueue<MESSAGE *>	_MessageQueue;


	/////////////////////////////////////////////////////////////////////////////////
	// Client List
	/////////////////////////////////////////////////////////////////////////////////
	CLIENTLIST					_Client;


	/////////////////////////////////////////////////////////////////////////////////
	// 메모리 풀 - MESSAGE struct, CLIENT struct
	/////////////////////////////////////////////////////////////////////////////////
	CMemoryPool<MESSAGE>		_MessageMemoryPool;
	CMemoryPool<CLIENT>			_ClientMemoryPool;

	/////////////////////////////////////////////////////////////////////////////////
	// 섹터
	/////////////////////////////////////////////////////////////////////////////////
	SECTOR						_Sector[eSECTOR_MAX_Y][eSECTOR_MAX_X];
};

#endif