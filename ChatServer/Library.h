#ifndef __LIBRARY__H__
#define __LIBRARY__H__

#include				<stdio.h>
#include				<tchar.h>
#include				<WinSock2.h>
#include				<WS2tcpip.h>
#include				<mstcpip.h>
#include				<process.h>
#include				<strsafe.h>
#include				<mmsystem.h>
#include				<conio.h>
#include				<float.h>
#include				<time.h>

#include				<psapi.h>
#include				<dbghelp.h>
#include				<crtdbg.h>
#include				<tlhelp32.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "ImageHlp")
#pragma comment(lib, "psapi")

/*------------------------------------------------------------*/
// Config
/*------------------------------------------------------------*/
#include				"TextParser.h"
#include				"_Config.h"

/*------------------------------------------------------------*/
// CrashDump
/*------------------------------------------------------------*/
#include				"APIHook.h"
#include				"CrashDump.h"

/*------------------------------------------------------------*/
// Profiler
/*------------------------------------------------------------*/
#include				"ProFiler.h"

/*------------------------------------------------------------*/
// SystemLog
/*------------------------------------------------------------*/
#include				"SystemLog.h"

/*------------------------------------------------------------*/
// Stream Queue
/*------------------------------------------------------------*/
#include				"AyaStreamSQ.h"

/*------------------------------------------------------------*/
// Lockfree MemoryPool
// Lockfree Stack
// Lockfree Queue
/*------------------------------------------------------------*/
#include				"MemoryPool.h"
#include				"LockfreeStack.h"
#include				"LockfreeQueue.h"

/*------------------------------------------------------------*/
// Network Packet
/*------------------------------------------------------------*/
#include				"NPacket.h"

/*------------------------------------------------------------*/
// Server Module
// - Session
// - LanServer
// - NetServer
/*------------------------------------------------------------*/
#define					PROFILE_CHECK

#include				"Session.h"
#include				"LanServer.h"
#include				"NetServer.h"

#endif