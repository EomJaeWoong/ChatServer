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
	// �޽��� ����ü
	/*-----------------------------------------------------------------------------*/
	typedef struct st_MESSAGE
	{
		WORD			wType;

		__int64			iSessionID;
		CNPacket		*pPacket;

		LPVOID			_Debug;
	} MESSAGE;

	/*-----------------------------------------------------------------------------*/
	// Sector �ϳ��� ��ǥ ����
	/*-----------------------------------------------------------------------------*/
	typedef struct st_SECTOR_POS
	{
		short			iX;
		short			iY;

	} SECTORINFO;


	/*-----------------------------------------------------------------------------*/
	// Client ����ü
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
	// Ư�� ��ġ �ֺ��� 9�� ���� ����
	/*-----------------------------------------------------------------------------*/
	struct st_SECTOR_AROUND
	{
		int				iCount;
		st_SECTOR_POS	Around[9];
	};

	/*-----------------------------------------------------------------------------*/
	// Ÿ�� ����
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
	// ChatServer ����͸� ������
	/////////////////////////////////////////////////////////////////////////////////
	static unsigned __stdcall	MonitorThread_Chat(LPVOID MonitorParam);
	int							MonitorThread_Chat_Update();
	
	/////////////////////////////////////////////////////////////////////////////////
	// �޽��� ó��, ����
	/////////////////////////////////////////////////////////////////////////////////
	bool						CompleteMessage(MESSAGE *pMessage);

	MESSAGE*					MakeMessage_NewConnection(__int64 iSessionID);
	MESSAGE*					MakeMessage_Disconnection(__int64 iSessionID);
	MESSAGE*					MakeMessage_Packet(__int64 iSessionID, CNPacket *pPacket);

	/////////////////////////////////////////////////////////////////////////////////
	// ��Ŷ ó��, ����
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
	// Ŭ���̾�Ʈ ����, ����, ã��
	/////////////////////////////////////////////////////////////////////////////////
	bool						CreateClient(__int64 iSessionID);
	CLIENT*						SearchClient(__int64 iSessionID);
	bool						DeleteClient(__int64 iSessionID);

	/////////////////////////////////////////////////////////////////////////////////
	// ���� ���� �Լ�
	// GetSectorAround		- �ֺ� ���� ���
	// UpdateSector			- �ش� Ŭ���̾�Ʈ ���Ϳ� ���� ������Ʈ(����� �߰�)
	/////////////////////////////////////////////////////////////////////////////////
	void						GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND *pSectorAround);
	void						UpdateSector(CLIENT *pClient, WORD wNewSectorX, WORD wNewSectorY);

public :
	/////////////////////////////////////////////////////////////////////////////////
	// ����͸� �׸��
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
	// Update Thread�� Handle, Event
	/////////////////////////////////////////////////////////////////////////////////
	HANDLE						_hUpdateThread;
	HANDLE						_hUpdateEvent;

	/////////////////////////////////////////////////////////////////////////////////
	// ChatServer Monitor Thread
	/////////////////////////////////////////////////////////////////////////////////
	HANDLE						_hMonitorThreadChat;

	/////////////////////////////////////////////////////////////////////////////////
	// ��Ŷ�� ��� �޽��� ť
	/////////////////////////////////////////////////////////////////////////////////
	CLockfreeQueue<MESSAGE *>	_MessageQueue;


	/////////////////////////////////////////////////////////////////////////////////
	// Client List
	/////////////////////////////////////////////////////////////////////////////////
	CLIENTLIST					_Client;


	/////////////////////////////////////////////////////////////////////////////////
	// �޸� Ǯ - MESSAGE struct, CLIENT struct
	/////////////////////////////////////////////////////////////////////////////////
	CMemoryPool<MESSAGE>		_MessageMemoryPool;
	CMemoryPool<CLIENT>			_ClientMemoryPool;

	/////////////////////////////////////////////////////////////////////////////////
	// ����
	/////////////////////////////////////////////////////////////////////////////////
	SECTOR						_Sector[eSECTOR_MAX_Y][eSECTOR_MAX_X];
};

#endif