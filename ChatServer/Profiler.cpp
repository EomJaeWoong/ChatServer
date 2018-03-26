#include "stdafx.h"

///////////////////////////////////////////////////////////////////////
// TLS 플래그 배열 인덱스
///////////////////////////////////////////////////////////////////////
DWORD					_dwTlsIndex;


///////////////////////////////////////////////////////////////////////
// 프로파일링할 대상 샘플
///////////////////////////////////////////////////////////////////////
st_THREAD_SAMPLE		_stProfileThread[eMAX_THREAD_SAMPlE];


///////////////////////////////////////////////////////////////////////
// 고해상도 타이머 값(CPU의 클럭 수)
///////////////////////////////////////////////////////////////////////
LARGE_INTEGER			_IFrequency;
double					_dMicroFrequency;



///////////////////////////////////////////////////////////////////////
// 프로파일러 초기화
///////////////////////////////////////////////////////////////////////
bool			ProfileInit()
{
	///////////////////////////////////////////////////////////////////
	// 타이머 값 얻기
	///////////////////////////////////////////////////////////////////
	QueryPerformanceFrequency(&_IFrequency);
	_dMicroFrequency = (double)_IFrequency.QuadPart / (double)1000000;


	for (int iCnt = 0; iCnt < eMAX_THREAD_SAMPlE; iCnt++)
		_stProfileThread[iCnt].lThreadID = -1;

	///////////////////////////////////////////////////////////////////
	// TLS Alloc
	///////////////////////////////////////////////////////////////////
	_dwTlsIndex = TlsAlloc();
	if (TLS_OUT_OF_INDEXES == _dwTlsIndex)
		_exit(-1);

	return true;
}


///////////////////////////////////////////////////////////////////////
// 프로파일링 시작
///////////////////////////////////////////////////////////////////////
bool			ProfileBegin(WCHAR *pwSampleName)
{
	LARGE_INTEGER	liStartTime;
	st_SAMPLE		*pSample = nullptr;

	///////////////////////////////////////////////////////////////////
	// 샘플 얻기
	///////////////////////////////////////////////////////////////////
	GetSample(pwSampleName, &pSample);
	if (nullptr == pSample)
		return false;


	///////////////////////////////////////////////////////////////////
	// 시간 측정 시작
	///////////////////////////////////////////////////////////////////
	QueryPerformanceCounter(&liStartTime);

	///////////////////////////////////////////////////////////////////
	// Begin이 중복으로 열렸을 때
	///////////////////////////////////////////////////////////////////
	if (0 != pSample->liStartTime.QuadPart)
		return false;

	pSample->liStartTime = liStartTime;
}

///////////////////////////////////////////////////////////////////////
// 프로파일링 끝
///////////////////////////////////////////////////////////////////////
bool			ProfileEnd(WCHAR *pwSampleName)
{
	LARGE_INTEGER	liEndTime, liSampleTime;
	st_SAMPLE		*pSample = nullptr;
	DWORD			dwTlsIndex = -1;

	///////////////////////////////////////////////////////////////////
	// 카운터 측정
	///////////////////////////////////////////////////////////////////
	QueryPerformanceCounter(&liEndTime);

	///////////////////////////////////////////////////////////////////
	// 샘플 얻기
	///////////////////////////////////////////////////////////////////
	GetSample(pwSampleName, &pSample);
	if (nullptr == pSample)
		return false;


	///////////////////////////////////////////////////////////////////
	// 시간 구하기
	///////////////////////////////////////////////////////////////////
	liSampleTime.QuadPart =
		liEndTime.QuadPart - pSample->liStartTime.QuadPart;
		

	///////////////////////////////////////////////////////////////////
	// 최대 최소 시간
	///////////////////////////////////////////////////////////////////
	if (pSample->dMaxTime[1] < liSampleTime.QuadPart)
	{
		pSample->dMaxTime[0] = pSample->dMaxTime[1];
		pSample->dMaxTime[1] = (double)liSampleTime.QuadPart / _dMicroFrequency;
	}

	if (pSample->dMinTime[1] > liSampleTime.QuadPart)
	{
		pSample->dMinTime[0] = pSample->dMinTime[1];
		pSample->dMinTime[1] = (double)liSampleTime.QuadPart / _dMicroFrequency;
	}

	///////////////////////////////////////////////////////////////////
	// 누적시간 저장
	///////////////////////////////////////////////////////////////////
	pSample->dTotalSampleTime +=
		(double)liSampleTime.QuadPart / _dMicroFrequency;

	///////////////////////////////////////////////////////////////////
	// 콜 횟수 증가
	///////////////////////////////////////////////////////////////////
	pSample->lCallCount++;

	memset(&pSample->liStartTime, 0, sizeof(LARGE_INTEGER));

	return true;
}



///////////////////////////////////////////////////////////////////////
// 샘플 얻기
///////////////////////////////////////////////////////////////////////
bool			GetSample(WCHAR *pwSampleName, st_SAMPLE **pOutSample)
{
	st_SAMPLE *pSample;

	///////////////////////////////////////////////////////////////////
	// 해당 쓰레드의 TLS Index를 얻음
	// 셋팅되어 있지 않다면 샘플값을 세팅함
	///////////////////////////////////////////////////////////////////
	pSample = (st_SAMPLE *)TlsGetValue(_dwTlsIndex);
	if (nullptr == pSample)
	{
		pSample = new st_SAMPLE[eMAX_SAMPLE];

		///////////////////////////////////////////////////////////////
		// 샘플 배열 초기화
		///////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < eMAX_SAMPLE; iCnt++)
			memset(&pSample[iCnt], 0, sizeof(st_SAMPLE));

		///////////////////////////////////////////////////////////////
		// TLS에 값 셋팅
		///////////////////////////////////////////////////////////////
		if (false == TlsSetValue(_dwTlsIndex, (LPVOID)pSample))
			return false;

		///////////////////////////////////////////////////////////////
		// 출력을 위해 ThreadSample 배열에도 저장
		///////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < eMAX_THREAD_SAMPlE; iCnt++)
		{
			if ((-1 == _stProfileThread[iCnt].lThreadID) &&
				(nullptr == _stProfileThread[iCnt].pSample))
			{
				_stProfileThread[iCnt].pSample = pSample;
				_stProfileThread[iCnt].lThreadID = GetCurrentThreadId();
				break;
			}
		}
	}

	///////////////////////////////////////////////////////////////////
	// 샘플 찾기
	///////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < eMAX_SAMPLE; iCnt++)
	{
		if ((pSample[iCnt].bUseFlag) && (0 == wcscmp(pSample[iCnt].wName, pwSampleName)))
		{
			*pOutSample = &pSample[iCnt];
			break;;
		}

		/////////////////////////////////////////////////////////
		// 해당 쓰레드 안에 샘플이 없을 때
		/////////////////////////////////////////////////////////
		if (0 == wcscmp(pSample[iCnt].wName, L""))
		{
			StringCchPrintf(pSample[iCnt].wName,
				wcslen(pwSampleName)+1,
				L"%s",
				pwSampleName);
			
			pSample[iCnt].bUseFlag = true;

			pSample[iCnt].liStartTime.QuadPart = 0;
			pSample[iCnt].dTotalSampleTime = 0;

			pSample[iCnt].dMaxTime[0] = 0;
			pSample[iCnt].dMinTime[0] = DBL_MAX;

			pSample[iCnt].dMaxTime[1] = 0;
			pSample[iCnt].dMinTime[1] = DBL_MAX;

			pSample[iCnt].lCallCount = 0;

			*pOutSample = &pSample[iCnt];

			break;
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////
// 프로파일링한 데이터 저장
///////////////////////////////////////////////////////////////////////
bool			SaveProfile()
{
	SYSTEMTIME		stNowTime;
	WCHAR			fileName[256];
	DWORD			dwBytesWritten;
	WCHAR			wBuffer[200];
	HANDLE			hFile;

	///////////////////////////////////////////////////////////////////
	// 파일 생성
	///////////////////////////////////////////////////////////////////
	GetLocalTime(&stNowTime);
	StringCchPrintf(fileName,
		sizeof(fileName),
		L"%4d-%02d-%02d %02d.%02d ProFiling Data.txt",
		stNowTime.wYear,
		stNowTime.wMonth,
		stNowTime.wDay,
		stNowTime.wHour,
		stNowTime.wMinute
		);

	///////////////////////////////////////////////////////////////////
	// 파일 만들기
	///////////////////////////////////////////////////////////////////
	hFile = ::CreateFile(fileName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_DELETE | FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	::WriteFile(hFile,
		L"﻿ ThreadID |                Name  |           Average  |            Min   |            Max   |          Call |\r\n",
		112 * sizeof(WCHAR), &dwBytesWritten, NULL);
	::WriteFile(hFile,
		L"-------------------------------------------------------------------------------------------------------------\r\n",
		111 * sizeof(WCHAR), &dwBytesWritten, NULL);

	for (int iThCnt = 0; eMAX_THREAD_SAMPlE; iThCnt++)
	{
		if (-1 == _stProfileThread[iThCnt].lThreadID)
			break;

		for (int iSampleCnt = 0; iSampleCnt < eMAX_THREAD_SAMPlE; iSampleCnt++)
		{
			if (false == _stProfileThread[iThCnt].pSample[iSampleCnt].bUseFlag)
				break;
			
			StringCchPrintf(wBuffer,
				sizeof(wBuffer),
				L"%9d | %20s | %16.4f㎲ | %14.4f㎲ | %14.4f㎲ | %13d |\r\n",
				_stProfileThread[iThCnt].lThreadID,
				_stProfileThread[iThCnt].pSample[iSampleCnt].wName,
				_stProfileThread[iThCnt].pSample[iSampleCnt].dTotalSampleTime 
				/ _stProfileThread[iThCnt].pSample[iSampleCnt].lCallCount,
				_stProfileThread[iThCnt].pSample[iSampleCnt].dMinTime[1],
				_stProfileThread[iThCnt].pSample[iSampleCnt].dMaxTime[1],
				_stProfileThread[iThCnt].pSample[iSampleCnt].lCallCount
				);
			::WriteFile(hFile, wBuffer, wcslen(wBuffer) * sizeof(WCHAR), &dwBytesWritten, NULL);
		}
		::WriteFile(hFile,
			L"-------------------------------------------------------------------------------------------------------------\r\n",
			111 * sizeof(WCHAR), &dwBytesWritten, NULL);
	}

	return true;
}

