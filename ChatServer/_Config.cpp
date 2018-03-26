#include "stdafx.h"

WCHAR	CConfigData::m_Network_Chat_BindIP[9] = { '\0', };
int		CConfigData::m_Network_Chat_Bind_Port = 0;
BOOL	CConfigData::m_Network_Chat_Nagle = 0;

int		CConfigData::m_System_Worker_Thread_Num = 0;
int		CConfigData::m_System_Max_Client = 0;

BYTE	CConfigData::m_System_Packet_Code = 0;
BYTE	CConfigData::m_System_Packet_Key1 = 0;
BYTE	CConfigData::m_System_Packet_Key2 = 0;

DWORD	CConfigData::m_System_Timeout_Time = 0;

BYTE	CConfigData::m_System_Max_ID = 0;
BYTE	CConfigData::m_System_Max_Nickname = 0;

BYTE	CConfigData::m_Sector_X_Max = 0;
BYTE	CConfigData::m_Sector_Y_Max = 0;