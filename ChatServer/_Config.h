#ifndef __CONFIG__H__
#define __CONFIG__H__

class CConfigData
{
public :
	CConfigData()
	{
		int iStrSize;

		_TextParser.LoadFile("_Config.ini");

		_TextParser.SearchZone("NETWORK");

		iStrSize = sizeof(m_Network_Chat_BindIP);
		_TextParser.GetValue("CHAT_BIND_IP", m_Network_Chat_BindIP, &iStrSize);
		_TextParser.GetValue("CHAT_BIND_PORT", &m_Network_Chat_Bind_Port);
		_TextParser.GetValue("CHAT_NAGLE", &m_Network_Chat_Nagle);

		_TextParser.SearchZone("SYSTEM");

		_TextParser.GetValue("WORKER_THREAD", &m_System_Worker_Thread_Num);
		_TextParser.GetValue("CLIENT_MAX", &m_System_Max_Client);

		_TextParser.GetValue("PACKET_CODE", &m_System_Packet_Code);
		_TextParser.GetValue("PACKET_KEY1", &m_System_Packet_Key1);
		_TextParser.GetValue("PACKET_KEY2", &m_System_Packet_Key2);

		_TextParser.GetValue("TIMEOUT_TIME", &m_System_Timeout_Time);

		_TextParser.GetValue("MAX_ID", &m_System_Max_ID);
		_TextParser.GetValue("MAX_NICKNAME", &m_System_Max_Nickname);

		_TextParser.GetValue("SECTOR_X_MAX", &m_Sector_X_Max);
		_TextParser.GetValue("SECTOR_Y_MAX", &m_Sector_Y_Max);
	}

public:
	static WCHAR	m_Network_Chat_BindIP[9];
	static int		m_Network_Chat_Bind_Port;
	static BOOL		m_Network_Chat_Nagle;

	static int		m_System_Worker_Thread_Num;
	static int		m_System_Max_Client;

	static BYTE		m_System_Packet_Code;
	static BYTE		m_System_Packet_Key1;
	static BYTE		m_System_Packet_Key2;

	static DWORD	m_System_Timeout_Time;

	static BYTE		m_System_Max_ID;
	static BYTE		m_System_Max_Nickname;

	static BYTE		m_Sector_X_Max;
	static BYTE		m_Sector_Y_Max;

private :
	CTextParser _TextParser;
};

#endif