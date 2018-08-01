#ifndef __SYSTEMLOG__H__
#define __SYSTEMLOG__H__

#include <time.h>
#include <direct.h>

typedef enum en_LOG_LEVEL{
	MODE_CONSOLE = 1,
	MODE_FILE = 2,
	LEVEL_SYSTEM = 10,
	LEVEL_ERROR = 20,
	LEVEL_WARNING = 30,
	LEVEL_DEBUG = 40,
}LOG;


class CSystemLog
{
public:
	CSystemLog()
	{
		_iLogCount = 0;
		_chLogMode = MODE_FILE;
	}

	virtual ~CSystemLog(){}

	static bool		SetLogLevel(char chLogLevel){
		_chLogMode = MODE_FILE;
		_chLogLevel = chLogLevel;
		return true;
	}

	static bool SetLogDirectory(WCHAR *wLogDirectory){
		memset(_szLogDir, 0, 256);
		memcpy(_szLogDir, wLogDirectory, sizeof(WCHAR) * wcslen(wLogDirectory));

		_wmkdir(_szLogDir);

		return true;
	}

	//---------------------------------------------------------------------------------
	// 로그 찍는 함수
	//---------------------------------------------------------------------------------
	static bool Log(WCHAR *szType, en_LOG_LEVEL enLogLevel, LPCTSTR szStringFormat, ...)
	{
		WCHAR *wLogLevel = NULL;
		WCHAR szInMessage[1024];
		DWORD dwBytesWritten;

		///////////////////////////////////////////////////////////////////////////////
		// 현재 시간 설정
		///////////////////////////////////////////////////////////////////////////////
		time_t timer;
		tm today;

		time(&timer);

		localtime_s(&today, &timer); // 초 단위의 시간을 분리하여 구조체에 넣기

		///////////////////////////////////////////////////////////////////////////////
		// 가변인자
		///////////////////////////////////////////////////////////////////////////////
		va_list va;
		va_start(va, szStringFormat);
		StringCchVPrintf(szInMessage, 256, szStringFormat, va);
		va_end(va);

		InterlockedIncrement64((LONG64 *)&_iLogCount);

		switch (enLogLevel)
		{
		case LEVEL_SYSTEM :
			wLogLevel = L"SYSTEM";
			break;

		case LEVEL_ERROR :
			wLogLevel = L"ERROR";
			break;

		case LEVEL_WARNING :
			wLogLevel = L"WARNING";
			break;

		case LEVEL_DEBUG :
			wLogLevel = L"DEBUG";
			break;

		default :
			return false;
		}

		if (_chLogLevel >= enLogLevel)
		{
			memset(_szLogBuffer, 0, 1024);

			StringCchPrintf(_szLogBuffer, 1024, L"[%s] [%04d-%02d-%02d %02d:%02d:%02d / %8s] [%08I64d] %s \r\n",
				szType,
				today.tm_year + 1900,
				today.tm_mon + 1,
				today.tm_mday,
				today.tm_hour,
				today.tm_min,
				today.tm_sec,
				wLogLevel,
				_iLogCount,
				szInMessage
				);

			////////////////////////////////////////////////////////////////
			// CONSOLE
			////////////////////////////////////////////////////////////////
			if (_chLogMode & 0x1)
			{
				wprintf(L"%s", _szLogBuffer);
			}

			////////////////////////////////////////////////////////////////
			// FILE
			////////////////////////////////////////////////////////////////
			if (_chLogMode & 0x2)
			{
				unsigned short mark = 0xFEFF;
				WCHAR szFileName[256];
				WCHAR szFileDerectory[256];
				StringCchPrintf(szFileName, 256, L"%d%02d_%s.txt", today.tm_year + 1900, today.tm_mon + 1, szType);
				StringCchPrintf(szFileDerectory, 256, L"%s\\%s", _szLogDir, szFileName);

				HANDLE hFile = ::CreateFile(szFileDerectory,
					GENERIC_WRITE,
					NULL,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
				SetFilePointer(hFile, 0, NULL, FILE_END);

				::WriteFile(hFile, &mark, sizeof(mark), &dwBytesWritten, NULL);
				if (!(::WriteFile(hFile, _szLogBuffer, (DWORD)(wcslen(_szLogBuffer) * sizeof(WCHAR)), &dwBytesWritten, NULL)))
					return false;
				CloseHandle(hFile);

			}
		}
		return true;
	}

	static void PrintToHex(WCHAR *szType, en_LOG_LEVEL chLogLevel, BYTE *byHexData, DWORD dwSize)
	{
		WCHAR *wLogLevel = NULL;
		WCHAR szInMessage[1024];
		DWORD dwBytesWritten;

		///////////////////////////////////////////////////////////////////////////////
		// 현재 시간 설정
		///////////////////////////////////////////////////////////////////////////////
		time_t timer;
		tm today;

		time(&timer);

		localtime_s(&today, &timer); // 초 단위의 시간을 분리하여 구조체에 넣기

		InterlockedIncrement64((LONG64 *)&_iLogCount);

		switch (chLogLevel)
		{
		case LEVEL_SYSTEM:
			wLogLevel = L"SYSTEM";
			break;

		case LEVEL_ERROR:
			wLogLevel = L"ERROR";
			break;

		case LEVEL_WARNING:
			wLogLevel = L"WARNING";
			break;

		case LEVEL_DEBUG:
			wLogLevel = L"DEBUG";
			break;

		default:
			return;
		}

		if (_chLogLevel >= chLogLevel)
		{
			for (int iCnt = 0; iCnt < dwSize; iCnt++){
				StringCchPrintf((STRSAFE_LPWSTR)&szInMessage[iCnt * 3], 1024, L"%02X", byHexData[iCnt]);
				StringCchPrintf((STRSAFE_LPWSTR)&szInMessage[iCnt * 3 + 2], 1024, L" ");
			}
			memset(_szLogBuffer, 0, 1024);

			StringCchPrintf(_szLogBuffer, 1024, L"[%s] [%04d-%02d-%02d %02d:%02d:%02d / %8s] [%08I64d] %s \r\n",
				szType,
				today.tm_year + 1900,
				today.tm_mon + 1,
				today.tm_mday,
				today.tm_hour,
				today.tm_min,
				today.tm_sec,
				wLogLevel,
				_iLogCount,
				szInMessage
				);

			////////////////////////////////////////////////////////////////
			// CONSOLE
			////////////////////////////////////////////////////////////////
			if (_chLogMode & 0x1)
			{
				wprintf(L"%s", _szLogBuffer);
			}

			////////////////////////////////////////////////////////////////
			// FILE
			////////////////////////////////////////////////////////////////
			if (_chLogMode & 0x2)
			{
				unsigned short mark = 0xFEFF;
				WCHAR szFileName[256];
				WCHAR szFileDerectory[256];
				StringCchPrintf(szFileName, 256, L"%d%02d_%s.txt", today.tm_year + 1900, today.tm_mon + 1, szType);
				StringCchPrintf(szFileDerectory, 256, L"%s\\%s", _szLogDir, szFileName);

				HANDLE hFile = ::CreateFile(szFileDerectory,
					GENERIC_WRITE,
					NULL,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
				SetFilePointer(hFile, 0, NULL, FILE_END);

				::WriteFile(hFile, &mark, sizeof(mark), &dwBytesWritten, NULL);
				if (!(::WriteFile(hFile, _szLogBuffer, (DWORD)(wcslen(_szLogBuffer) * sizeof(WCHAR)), &dwBytesWritten, NULL)))
					return;
				CloseHandle(hFile);

			}
		}
		return;
	}

	/*
	
	void PrintToSessionKey64();
	*/

	static __int64			_iLogCount;

	static char				_chLogMode;

	static char				_chLogLevel;
	static WCHAR			_szLogDir[256];

	static WCHAR			_szLogBuffer[1024];
};

#define LOG(szType, enLogLevel, szStringFormat, ...)		CSystemLog::Log(szType, enLogLevel, szStringFormat, __VA_ARGS__)
#define LOG_HEX(szType, enLogLevel, byHexData, dwSize)		CSystemLog::PrintToHex(szType, enLogLevel, byHexData, dwSize)
#define SYSLOG_DIRECTORY(DIRECTORY)							CSystemLog::SetLogDirectory(DIRECTORY)
#define SYSLOG_LEVEL(LEVEL)									CSystemLog::SetLogLevel(LEVEL)

#endif