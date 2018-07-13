#include "stdafx.h"

__int64			CSystemLog::_iLogCount = 0;
char			CSystemLog::_chLogMode;
char			CSystemLog::_chLogLevel;
WCHAR			CSystemLog::_szLogDir[256];
WCHAR			CSystemLog::_szLogBuffer[256];