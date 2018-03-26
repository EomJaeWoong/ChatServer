/////////////////////////////////////////////////////////////////////
// www.gamecodi.com						이주행 master@gamecodi.com
//
//
/////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------

	Aya Library - Stream SingleQueue

	원형 FIFO 스트리밍 큐.


	완성된 패킷을 만들기 위해서 StreamDQ 앞단에 사용을 목적으로 한다.


	- 사용법.

	CAyaStreamSQ	clStreamQ;
	CAyaStreamSQ	clStreamQ(1024);




----------------------------------------------------------------*/
#ifndef __AYA_STREAM_SINGLE_QUEUE__
#define __AYA_STREAM_SINGLE_QUEUE__

class CAyaStreamSQ
{
public:

	/*---------------------------------------------------------------
	AyaStreamSQ Enum.

	----------------------------------------------------------------*/
	enum e_AYA_STREAM_SQ
	{
		eBUFFER_DEFAULT				= 20960,		// 버퍼의 기본 크기.
		eBUFFER_BLANK				= 8			 	// 확실한 구분을 위해 8Byte 의 빈공간.

	};

public:

	/////////////////////////////////////////////////////////////////////////
	// 생성자(임의 크기 지정), 파괴자.
	//
	//
	/////////////////////////////////////////////////////////////////////////
	CAyaStreamSQ(void)
	{
		Initial(eBUFFER_DEFAULT);
	}


	CAyaStreamSQ(int iBufferSize)
	{
		Initial(iBufferSize);
	}

	virtual	~CAyaStreamSQ(void)
	{
		delete[] m_chpBuffer;

		DeleteCriticalSection(&m_csQueue);
	}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼 생성 & 큐의 초기화.
	//
	// Parameters: (int)버퍼용량.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	void	Initial(int iBufferSize)
	{
		m_iBufferSize = iBufferSize;

		m_chpBuffer = (char *)malloc(iBufferSize);

		m_iReadPos = 0;
		m_iWritePos = 0;

		InitializeCriticalSection(&m_csQueue);
	}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼 전체의 용량 얻음.
	//
	// Parameters: 없음.
	// Return: (int)버퍼용량.
	/////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void){	return m_iBufferSize;	}

	/////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 용량 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 용량.
	/////////////////////////////////////////////////////////////////////////
	int		GetUseSize(void)
	{
		if (m_iReadPos <= m_iWritePos)
			return m_iWritePos - m_iReadPos;

		else
			return (m_iBufferSize - m_iReadPos) + m_iWritePos;
	}

	/////////////////////////////////////////////////////////////////////////
	// 현재 버퍼에 남은 용량 얻기.
	//
	// Parameters: 없음.
	// Return: (int)남은용량.
	/////////////////////////////////////////////////////////////////////////
	int		GetFreeSize(void)
	{
		if (m_iReadPos > m_iWritePos)
			return m_iReadPos - m_iWritePos - eBUFFER_BLANK;

		else
			return (m_iBufferSize - m_iWritePos) + m_iReadPos - eBUFFER_BLANK;
	}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
	// (끊기지 않은 길이)
	//
	// Parameters: 없음.
	// Return: (int)사용가능 용량.
	/////////////////////////////////////////////////////////////////////////
	int		GetNotBrokenGetSize(void)
	{
		if (m_iReadPos <= m_iWritePos)
			return m_iWritePos - m_iReadPos;

		else
			return m_iBufferSize - m_iReadPos;
	}

	int		GetNotBrokenPutSize(void)
	{
		if (m_iReadPos > m_iWritePos)
			return m_iReadPos - m_iWritePos - eBUFFER_BLANK;

		else
		{
			if (m_iReadPos < eBUFFER_BLANK)
				return m_iBufferSize - m_iWritePos - (eBUFFER_BLANK - m_iReadPos);
			else
				return m_iBufferSize - m_iWritePos;
		}
			
	}


	/////////////////////////////////////////////////////////////////////////
	// WritePos 에 데이타 넣음.
	//
	// Parameters: (char *)데이타 포인터. (int)크기. 
	// Return: (int)넣은 크기.
	/////////////////////////////////////////////////////////////////////////
	int		Put(char *chpData, int iSize)
	{
		if (iSize > GetFreeSize())
			iSize = GetFreeSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			m_chpBuffer[m_iWritePos] = chpData[iCnt];
			m_iWritePos = (m_iWritePos + 1) % m_iBufferSize;
		}

		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos 에서 데이타 가져옴. ReadPos 이동.
	//
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	/////////////////////////////////////////////////////////////////////////
	int		Get(char *chpDest, int iSize)
	{
		if (iSize > GetUseSize())
			iSize = GetUseSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			chpDest[iCnt] = m_chpBuffer[m_iReadPos];
			m_iReadPos = (m_iReadPos + 1) % m_iBufferSize;
		}

		return iSize; 
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
	//
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	/////////////////////////////////////////////////////////////////////////
	int		Peek(char *chpDest, int iSize)
	{
		if (iSize > GetUseSize())
			iSize = GetUseSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			chpDest[iCnt] = m_chpBuffer[(m_iReadPos + iCnt) % m_iBufferSize];
		}

		return iSize;
	}





	/////////////////////////////////////////////////////////////////////////
	// 원하는 길이만큼 읽기위치 에서 삭제.
	//
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	void		RemoveData(int iSize)
	{
		if (GetUseSize() < iSize)
			iSize = GetUseSize();
		
		m_iReadPos = (m_iReadPos + iSize) % m_iBufferSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// Write 의 위치를 이동하는 함수.
	//
	// Parameters: 없음.
	// Return: (int)Write 이동 사이즈
	/////////////////////////////////////////////////////////////////////////
	int	MoveWritePos(int iSize)
	{
		if (GetFreeSize() < iSize)
			iSize = GetFreeSize();
		
		m_iWritePos = (m_iWritePos + iSize) % m_iBufferSize;

		return iSize;
	}


	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 모든 데이타 삭제.
	//
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	void	ClearBuffer(void)
	{
		m_iReadPos = 0;
		m_iWritePos = 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char	*GetBufferPtr(void){	return m_chpBuffer;		}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 ReadPos 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char	*GetReadBufferPtr(void){	return &m_chpBuffer[m_iReadPos];	 }

	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 WritePos 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char	*GetWriteBufferPtr(void){ return &m_chpBuffer[m_iWritePos];		 }



	/////////////////////////////////////////////////////////////////////////
	// Lock, Unlock.
	// 스레드 환경을 위해 CriticalSection 으로 Lock, Unlock 처리.
	// 
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	void	Lock(void){		EnterCriticalSection(&m_csQueue);	}
	void	Unlock(void){	LeaveCriticalSection(&m_csQueue);	}





protected:

	//------------------------------------------------------------
	// 버퍼 포인터.
	//------------------------------------------------------------
	char				*m_chpBuffer;
	//------------------------------------------------------------
	// 버퍼 사이즈.
	//------------------------------------------------------------
	int					m_iBufferSize;

	public :

	//------------------------------------------------------------
	// 버퍼의 읽기 위치, 쓰기 위치.
	//------------------------------------------------------------
	int					m_iReadPos;
	int					m_iWritePos;

	//------------------------------------------------------------
	// 스레드 환경을 위해 크리티걸 섹션 사용.
	//------------------------------------------------------------
	CRITICAL_SECTION	m_csQueue;


};


#endif