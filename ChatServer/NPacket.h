/*---------------------------------------------------------------

N Packet.

네트워크 패킷용 클래스.
간편하게 패킷에 순서대로 데이타를 In, Out 한다.

플랫폼에 따라서 변수별 사이즈가 다르므로, 네트워크 패킷 전송시에 문제가 발생할 수 있다.
그리하여 플랫폼별 차이점을 위한 고정사항은 다음과 같다.

--------------------------------------------------------

NPacket::_ValueSizeCheck()

함수를 최초 1회 호출하여 각 변수별 사이즈 확인을 필수로 해야한다.
FALSE 리턴시 개발자에게 상의해라.


아래 사이즈와 다를 경우 컴파일 시에러를 발생한다.

C					C#				TYPE
char				sbyte			signed 1Byte
unsigned char		byte			unsigned 1Byte
WCHAR				char			unicode char 2Byte

short				short			signed 2Byte
unsigned short		ushort			unsigned 2Byte

int					int				signed 4Byte
unsigned int		uint			unsigned 4Byte
float				float			float 4Byte

__int64				long			signed 8Byte
unsigned __int64	ulong			unsigned 8Byte

--------------------------------------------------------


- 사용법.

CNPacket cPacket;

넣기.
clPacket << 40030;	or	clPacket << iValue;		(int 넣기)
clPacket << 3;		or	clPacket << byValue;	(BYTE 넣기)
clPacket << 1.4;	or	clPacket << fValue;		(float 넣기)

빼기.
clPacket >> iValue;		(int 빼기)
clPacket >> byValue;	(BYTE 빼기)
clPacket >> fValue;		(float 빼기)

!.	삽입되는 데이타 FIFO 순서로 관리된다.
큐가 아니므로, 넣기(<<).빼기(>>) 를 혼합해서 사용하면 안된다.

----------------------------------------------------------------*/
#ifndef  __N_PACKET__
#define  __N_PACKET__

class CNPacket
{
public:

	/*---------------------------------------------------------------
	AyaPacket Enum.

	----------------------------------------------------------------*/
	enum enN_PACKET
	{
		eBUFFER_DEFAULT = 10000,	// 패킷의 기본 버퍼 사이즈.
		eSTRING_MAX = 1024,			// 문자열 입력시 최대길이
		eBUFFER_HEADER_SIZE = 5,	// 헤더의 사이즈

		eSBYTE = 1,
		eBYTE = 1,
		eWCHAR = 2,

		eSHORT = 2,
		eUSHORT = 2,
		eINT = 4,
		eUINT = 4,
		eFLOAT = 4,

		eINT64 = 8,		// C# 에선 long 임.
		eUINT64 = 8
	};

	/*-----------------------------------------------------------------------*/
	// 암호화된 헤더 구조
	//
	// Code(1byte) - Len(2byte) - Rand XOR Code(1byte) - CheckSum(1byte) - Payload(Len byte)
	/*-----------------------------------------------------------------------*/
	#pragma pack(push, 1)
	typedef struct stEncodeHeader
	{
		BYTE byPacketCode;

		WORD shPacketLen;

		BYTE byRandXORCode;
		BYTE byCheckSum;
	} EncodeHeader;
	#pragma pack(pop)

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:
	//				---------------------------------------------------------
	//				(int) 버퍼 사이즈.
	//				---------------------------------------------------------
	//				(const CNPacket &) 패킷의 복사를 위한 패킷 레퍼런스.
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CNPacket()
	{
		Initial();
	}


	CNPacket(int iBufferSize)
	{
		Initial(iBufferSize);
	}


	CNPacket(const CNPacket &clSrcPacket)
	{
		m_chpBuffer = new unsigned char[clSrcPacket.m_iBufferSize];
		StringCchPrintfA((STRSAFE_LPSTR)m_chpBuffer,
			clSrcPacket.m_iBufferSize,
			(STRSAFE_LPCSTR)clSrcPacket.m_chpBuffer
			);

		m_iBufferSize = clSrcPacket.m_iBufferSize;

		m_chpDataFieldStart = m_chpBuffer;
		m_chpDataFieldEnd = m_chpBuffer;

		m_chpReadPos = m_chpBuffer + (clSrcPacket.m_chpReadPos - clSrcPacket.m_chpBuffer);
		m_chpWritePos = m_chpBuffer + (clSrcPacket.m_chpWritePos - clSrcPacket.m_chpBuffer);

		m_iDataSize = clSrcPacket.m_iDataSize;
	}

	virtual			~CNPacket()
	{
		Release();
	}


	//////////////////////////////////////////////////////////////////////////
	// 현 시스템의 변수 길이 체크.
	//
	// Parameters: 없음.
	// Return: (bool)true,false.
	//////////////////////////////////////////////////////////////////////////
	static bool		_ValueSizeCheck(void)
	{
		// 1 byte 변수 
		if (eSBYTE != sizeof(char) || eBYTE != sizeof(unsigned char)) return false;

		// 2 byte 변수 
		if (eWCHAR != sizeof(WCHAR) || eSHORT != sizeof(short) || eUSHORT != sizeof(unsigned short)) return false;

		// 4 byte 변수 
		if (eINT != sizeof(int) || eUINT != sizeof(unsigned int) || eFLOAT != sizeof(float)) return false;

		// 8 byte 변수 
		if (eINT64 != sizeof(__int64) || eUINT64 != sizeof(unsigned __int64)) return false;
		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 초기화.
	//
	// 메모리 할당을 여기서 하므로, 함부로 호출하면 안된다. 
	//
	// Parameters: (int)BufferSize.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void			Initial(int iBufferSize = eBUFFER_DEFAULT)
	{
		m_chpBuffer = new unsigned char[iBufferSize];

		m_iBufferSize = iBufferSize - 5;

		m_chpDataFieldStart = m_chpBuffer + 5;
		m_chpDataFieldEnd = m_chpDataFieldStart;

		m_chpReadPos = m_chpDataFieldStart;
		m_chpWritePos = m_chpDataFieldStart;

		m_bEncode = false;

		m_iDataSize = 0;

		m_lRefCnt = 0;

		m_byPacketCode = CConfigData::m_System_Packet_Code;

		m_byXORCode1 = CConfigData::m_System_Packet_Key1;
		m_byXORCode2 = CConfigData::m_System_Packet_Key2;
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷  파괴.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void			Release(void)
	{
		delete[] m_chpBuffer;
	}



	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void			Clear(void)
	{
		m_chpReadPos = m_chpDataFieldStart;
		m_chpWritePos = m_chpDataFieldStart;
		m_chpDataFieldEnd = m_chpDataFieldStart;

		m_iDataSize = 0;

		m_bEncode = false;
	}



	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int				GetBufferSize(void) { return m_iBufferSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	const int		GetDataSize(void) const { return m_iDataSize; }



	//////////////////////////////////////////////////////////////////////////
	// 버퍼(데이터) 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (unsigned char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	unsigned char	*GetBufferPtr(void) const { return m_chpDataFieldStart; }



	//////////////////////////////////////////////////////////////////////////
	// 버퍼(헤더) 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (unsigned char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	unsigned char	*GetBufferHeaderPtr(void) const { return m_chpReadPos; }



	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int				MoveWritePos(int iSize)
	{
		if (0 > iSize)		return 0;

		unsigned int iWriteIndex = m_chpWritePos - m_chpBuffer;

		if ((iWriteIndex + iSize) > m_iBufferSize)			return 0;

		m_chpWritePos += iSize;
		m_chpDataFieldEnd += iSize;
		m_iDataSize += iSize;

		return iSize;
	}

	int				MoveReadPos(int iSize)
	{
		if (0 > iSize)		return 0;

		unsigned int iReadIndex = m_chpWritePos - m_chpBuffer;

		if ((iReadIndex + iSize) > m_iBufferSize)			return 0;

		m_chpReadPos += iSize;
		m_iDataSize -= iSize;

		return iSize;
	}


	//////////////////////////////////////////////////////////////////////////
	// 헤더 셋팅(5Byte)
	// 패킷 헤더 셋팅. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	void			SetHeader(char *pHeader)
	{
		memcpy_s(m_chpBuffer, eBUFFER_HEADER_SIZE, pHeader, eBUFFER_HEADER_SIZE);

		m_chpReadPos -= eBUFFER_HEADER_SIZE;
		m_iDataSize += eBUFFER_HEADER_SIZE;
	}


	//////////////////////////////////////////////////////////////////////////
	// 헤더 셋팅(일단 5Byte 이하로)
	// 패킷 헤더 셋팅. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	void			SetCustomHeader(char *pHeader, int iCustomHeaderSize)
	{
		if (iCustomHeaderSize > 5)				return;

		unsigned char * chpHeaderPos = m_chpBuffer + iCustomHeaderSize;

		memcpy_s(chpHeaderPos, iCustomHeaderSize, pHeader, iCustomHeaderSize);

		m_chpReadPos -= iCustomHeaderSize;
		m_iDataSize += iCustomHeaderSize;
	}


	//////////////////////////////////////////////////////////////////////////
	// 헤더 셋팅(2Byte, 에코 서버용)
	// 패킷 헤더 셋팅. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	void			SetCustomShortHeader(unsigned short Header)
	{
		unsigned short * spHeaderPos = (unsigned short *)(m_chpBuffer + 3);

		*spHeaderPos = Header;

		m_chpReadPos -= sizeof(unsigned short);
		m_iDataSize += sizeof(unsigned short);
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 할당.
	// 패킷을 메모리 풀에서 할당 받아서 사용. 
	//
	// Parameters: 없음.
	// Return: (CNPacket *) 패킷 포인터.
	//////////////////////////////////////////////////////////////////////////
	static CNPacket *Alloc()
	{
		CNPacket *pAllocPacket = m_PacketPool.Alloc();

		if (NULL == pAllocPacket)
			CCrashDump::Crash();

		pAllocPacket->addRef();
		pAllocPacket->Clear();

		return pAllocPacket;
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 반환.
	// 패킷을 메모리 풀에 반환. 
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void			Free()
	{
		int result = InterlockedDecrement((LONG *)&m_lRefCnt);

		if (0 == result)
		{
			this->~CNPacket();

			m_PacketPool.Free(this);
		}

		else if(0 > result)
			CCrashDump::Crash();
	}


	//////////////////////////////////////////////////////////////////////////
	// 할당 카운트 증가. 
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void			addRef()
	{
		InterlockedIncrement((LONG *)&m_lRefCnt);
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 암호화
	//
	// Parameters : 없음.
	// return : (bool)성공 여부.
	//////////////////////////////////////////////////////////////////////////
	bool			Encode()
	{
		EncodeHeader eHeader;

		if (true == InterlockedCompareExchange((long *)&m_bEncode, true, false))
			return false;
		
		//////////////////////////////////////////////////////////////////////
		// Packet Code 넣기
		//////////////////////////////////////////////////////////////////////
		eHeader.byPacketCode = m_byPacketCode;

		//////////////////////////////////////////////////////////////////////
		// Payload 길이 넣기
		//////////////////////////////////////////////////////////////////////
		eHeader.shPacketLen = m_iDataSize;

		//////////////////////////////////////////////////////////////////////
		// 랜덤키 생성
		//////////////////////////////////////////////////////////////////////
		srand(time(NULL));
		eHeader.byRandXORCode = rand() % 256;


		//////////////////////////////////////////////////////////////////////
		// checkSum 생성
		//////////////////////////////////////////////////////////////////////
		DWORD		dwCheckSum = 0;
		for (BYTE *pbyPayloadPos = m_chpReadPos; 
			pbyPayloadPos < m_chpWritePos;
			pbyPayloadPos++)
			dwCheckSum += (int)*pbyPayloadPos;

		eHeader.byCheckSum = (BYTE)(dwCheckSum % 256);

		//////////////////////////////////////////////////////////////////////
		// Rand Xor Code로 CheckSum, Payload 바이트단위 xor
		//////////////////////////////////////////////////////////////////////
		eHeader.byCheckSum = eHeader.byCheckSum ^ eHeader.byRandXORCode;

		for (int iCnt = 0; iCnt < eHeader.shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				eHeader.byRandXORCode;
		}

		//////////////////////////////////////////////////////////////////////
		// 고정 XOR Code 1로 Rand XOR Code, CheckSum, Payload XOR
		//////////////////////////////////////////////////////////////////////
		eHeader.byRandXORCode = eHeader.byRandXORCode ^ m_byXORCode1;
		eHeader.byCheckSum = eHeader.byCheckSum ^ m_byXORCode1;

		for (int iCnt = 0; iCnt < eHeader.shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				m_byXORCode1;
		}

		//////////////////////////////////////////////////////////////////////
		// 고정 XOR Code 2로 Rand XOR Code, CheckSum, Payload XOR
		//////////////////////////////////////////////////////////////////////
		eHeader.byRandXORCode = eHeader.byRandXORCode ^ m_byXORCode2;
		eHeader.byCheckSum = eHeader.byCheckSum ^ m_byXORCode2;

		for (int iCnt = 0; iCnt < eHeader.shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				m_byXORCode2;
		}

		SetHeader((char *)&eHeader);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// 패킷 복호화
	//
	// Parameters : (char *)헤더 포인터.(Default = nullptr)
	// return : (bool)성공 여부.
	//////////////////////////////////////////////////////////////////////////
	bool			Decode(char *pHeader = nullptr)
	{
		EncodeHeader *peHeader = (EncodeHeader *)m_chpBuffer;

		//////////////////////////////////////////////////////////////////////
		// PacketCode와 길이 검사
		//////////////////////////////////////////////////////////////////////
		if (m_byPacketCode != peHeader->byPacketCode)
			return false;

		if (m_iDataSize - eBUFFER_HEADER_SIZE != peHeader->shPacketLen)
			return false;

		//////////////////////////////////////////////////////////////////////
		// 고정 XOR Code 2로 Rand XOR Code, CheckSum, Payload XOR
		//////////////////////////////////////////////////////////////////////
		peHeader->byRandXORCode = peHeader->byRandXORCode ^ m_byXORCode2;
		peHeader->byCheckSum = peHeader->byCheckSum ^ m_byXORCode2;

		for (int iCnt = 0; iCnt < peHeader->shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				m_byXORCode2;
		}

		//////////////////////////////////////////////////////////////////////
		// 고정 XOR Code 1로 Rand XOR Code, CheckSum, Payload XOR
		//////////////////////////////////////////////////////////////////////
		peHeader->byRandXORCode = peHeader->byRandXORCode ^ m_byXORCode1;
		peHeader->byCheckSum = peHeader->byCheckSum ^ m_byXORCode1;

		for (int iCnt = 0; iCnt < peHeader->shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				m_byXORCode1;
		}

		//////////////////////////////////////////////////////////////////////
		// Rand Xor Code로 CheckSum, Payload 바이트단위 xor
		//////////////////////////////////////////////////////////////////////
		peHeader->byCheckSum = peHeader->byCheckSum ^ peHeader->byRandXORCode;

		for (int iCnt = 0; iCnt < peHeader->shPacketLen; iCnt++)
		{
			*(m_chpDataFieldStart + iCnt) = *(m_chpDataFieldStart + iCnt) ^
				peHeader->byRandXORCode;
		}

		//////////////////////////////////////////////////////////////////////
		// checkSum 검사
		//////////////////////////////////////////////////////////////////////
		DWORD	dwCheckSum = 0;
		BYTE	byCheckSum = 0;

		for (BYTE *pbyPayloadPos = m_chpDataFieldStart;
			pbyPayloadPos < m_chpDataFieldEnd;
			pbyPayloadPos++)
			dwCheckSum += (DWORD)*pbyPayloadPos;

		byCheckSum = (BYTE)(dwCheckSum % 256);

		if (peHeader->byCheckSum != byCheckSum)
			CCrashDump::Crash();
		//	return false;


		memset(m_chpBuffer, 0, eBUFFER_HEADER_SIZE);
		m_chpReadPos += eBUFFER_HEADER_SIZE;
		m_iDataSize -= eBUFFER_HEADER_SIZE;

		InterlockedCompareExchange((long *)&m_bEncode, false, true);

		return true;
	}


	static long GetPacketCount()
	{
		return m_PacketPool.GetAllocCount();
	}


	/* ============================================================================= */
	// 연산자 오퍼레이터.
	/* ============================================================================= */
	CNPacket	&operator = (CNPacket &clSrcPacket)
	{
		m_chpBuffer = new unsigned char[clSrcPacket.m_iBufferSize];
		StringCchPrintfA((STRSAFE_LPSTR)m_chpBuffer,
			clSrcPacket.m_iBufferSize,
			(STRSAFE_LPCSTR)clSrcPacket.m_chpBuffer
			);

		m_iBufferSize = clSrcPacket.m_iBufferSize;

		m_chpDataFieldStart = m_chpBuffer;
		m_chpDataFieldEnd = m_chpBuffer;

		m_chpReadPos = m_chpBuffer + (clSrcPacket.m_chpReadPos - clSrcPacket.m_chpBuffer);
		m_chpWritePos = m_chpBuffer + (clSrcPacket.m_chpWritePos - clSrcPacket.m_chpBuffer);

		m_iDataSize = clSrcPacket.m_iDataSize;

		return *this;
	}
	
	void CNPacket::operator << (char chValue){ Put(chValue); }
	void CNPacket::operator << (unsigned char byValue){ Put(byValue); }
	void CNPacket::operator << (WCHAR wchValue){ Put(wchValue); }
	void CNPacket::operator << (short shValue){ Put(shValue); }
	void CNPacket::operator << (unsigned short wValue){ Put(wValue); }
	void CNPacket::operator << (int iValue){ Put(iValue); }
	void CNPacket::operator << (unsigned int iValue){ Put(iValue); }
	void CNPacket::operator << (float fValue){ Put(fValue); }
	void CNPacket::operator << (__int64 i64Value){ Put(i64Value); }
	void CNPacket::operator << (unsigned __int64 i64Value){ Put(i64Value); }
	void CNPacket::operator << (WCHAR *szString){ Put(szString); }

	void CNPacket::operator >> (char &chValue){ Get(chValue); }
	void CNPacket::operator >> (unsigned char &byValue){ Get(byValue); }
	void CNPacket::operator >> (WCHAR &wchValue){ Get(wchValue); }
	void CNPacket::operator >> (short &shValue){ Get(shValue); }
	void CNPacket::operator >> (unsigned short &wValue){ Get(wValue); }
	void CNPacket::operator >> (int &iValue){ Get(iValue); }
	void CNPacket::operator >> (unsigned int &iValue){ Get(iValue); }
	void CNPacket::operator >> (float &fValue){ Get(fValue); }
	void CNPacket::operator >> (__int64 &i64Value){ Get(i64Value); }
	void CNPacket::operator >> (unsigned __int64 &i64Value){ Get(i64Value); }
	void CNPacket::operator >> (WCHAR *szString){ Get(szString, eSTRING_MAX); }


	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	int Put(char chValue)				// C# - sbyte	// signed 1Byte
	{
		return PutData((unsigned char *)&chValue, eBYTE);
	}

	int Put(unsigned char byValue)		// C# - byte	// unsigned 1Byte
	{
		return PutData((unsigned char *)&byValue, eBYTE);
	}

	int Put(WCHAR wchValue)				// C# - char	// unicode char 2Byte
	{
		return PutData((unsigned char *)&wchValue, eWCHAR);
	}

	int Put(short shValue)				// C# - short	// signed 2Byte
	{
		return PutData((unsigned char *)&shValue, eSHORT);
	}

	int Put(unsigned short wValue)		// C# - ushort	// unsigned 2Byte
	{
		return PutData((unsigned char *)&wValue, eUSHORT);
	}

	int Put(int iValue)					// C# - int		// signed 4Byte
	{
		return PutData((unsigned char *)&iValue, eINT);
	}

	int Put(unsigned int iValue)		// C# - uint	// unsigned 4Byte
	{
		return PutData((unsigned char *)&iValue, eUINT);
	}
	int Put(float fValue)				// C# - float	// float 4Byte
	{
		return PutData((unsigned char *)&fValue, eFLOAT);
	}

	int Put(__int64 i64Value)			// C# - long	// signed 8Byte
	{
		return PutData((unsigned char *)&i64Value, eINT64);
	}

	int Put(unsigned __int64 i64Value)	// C# - ulong	// unsigned 8Byte
	{
		return PutData((unsigned char *)&i64Value, eUINT64);
	}

	int	Put(WCHAR *szString)
	{
		if (eSTRING_MAX < wcslen(szString))
			return 0;
		else
			return PutData((unsigned char *)szString, (wcslen(szString) + 1)* eWCHAR);
	}

	//int	Put(Proud::String &String);		// 문자열 넣기

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////

	int Get(unsigned char &byValue) const
	{
		return GetData((unsigned char *)&byValue, eBYTE);
	}

	int Get(char &chValue) const
	{
		return GetData((unsigned char *)&chValue, eBYTE);
	}

	int Get(WCHAR &wchValue) const
	{
		return GetData((unsigned char *)&wchValue, eWCHAR);
	}

	int Get(short &shValue) const
	{
		return GetData((unsigned char *)&shValue, eSHORT);
	}

	int Get(unsigned short &wValue) const
	{
		return GetData((unsigned char *)&wValue, eUSHORT);
	}

	int Get(int &iValue) const
	{
		return GetData((unsigned char *)&iValue, eINT);
	}

	int Get(unsigned int &iValue) const
	{
		return GetData((unsigned char *)&iValue, eUINT);
	}

	int Get(float &fValue) const
	{
		return GetData((unsigned char *)&fValue, eFLOAT);
	}

	int Get(__int64 &i64Value) const			// C# - long
	{
		return GetData((unsigned char *)&i64Value, eINT64);
	}

	int Get(unsigned __int64 &i64Value) const	// C# - ulong
	{
		return GetData((unsigned char *)&i64Value, eUINT64);
	}

	int	Get(WCHAR *szString, int iBuffer) const
	{
		int iCnt = 0;

		do{
			szString[iCnt] = *((WCHAR *)m_chpReadPos);
			m_chpReadPos += eWCHAR;
			m_iDataSize -= eWCHAR;
			iCnt++;
		} while (*((WCHAR *)m_chpReadPos) != L'\0');

		szString[iCnt] = L'\0';

		return iCnt * eWCHAR;
	}

	//int	Get(Proud::String &String) const;		// 문자열 빼기

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (unsigned char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int					GetData(unsigned char *bypDest, int iSize) const
	{
		int iCnt;

		if ((m_chpWritePos - m_chpReadPos) < iSize)				return 0;

		for (iCnt = 0; iCnt < iSize; iCnt++)
		{
			bypDest[iCnt] = *(m_chpReadPos++);
			m_iDataSize--;
		}

		return iCnt;
	}

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (unsigned char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int					PutData(unsigned char *bypSrc, int iSrcSize)
	{
		int iCnt;

		if (((m_chpBuffer + m_iBufferSize) - m_chpWritePos) < iSrcSize)
			return 0;

		for (iCnt = 0; iCnt < iSrcSize; iCnt++)
		{
			*(m_chpWritePos++) = bypSrc[iCnt];
			m_iDataSize++;
			m_chpDataFieldEnd++;
		}

		return iCnt;
	}

public :
	//------------------------------------------------------------
	// 암호화 여부 플래그
	//------------------------------------------------------------
	BOOL							m_bEncode;

protected:

	//------------------------------------------------------------
	// 패킷버퍼 / 버퍼 사이즈.
	//------------------------------------------------------------
	unsigned char					*m_chpBufferExpansion;

	unsigned char					*m_chpBuffer;
	int								m_iBufferSize;
	//------------------------------------------------------------
	// 패킷버퍼 시작 위치.	(본 클래스 에서는 사용하지 않지만, 확장성을 위해 사용)
	//------------------------------------------------------------
	unsigned char					*m_chpDataFieldStart;
	unsigned char					*m_chpDataFieldEnd;


	//------------------------------------------------------------
	// 버퍼의 읽을 위치, 넣을 위치.
	//------------------------------------------------------------
	mutable unsigned char			*m_chpReadPos;
	mutable unsigned char			*m_chpWritePos;


	//------------------------------------------------------------
	// 현재 버퍼에 사용중인 사이즈.
	//------------------------------------------------------------
	mutable int						m_iDataSize;


	//------------------------------------------------------------
	// 할당 카운터
	//------------------------------------------------------------
	long							m_lRefCnt;

	//------------------------------------------------------------
	// 패킷 메모리 풀
	//------------------------------------------------------------
	static CMemoryPool<CNPacket>	m_PacketPool;

	//------------------------------------------------------------
	// 1Byte 고정 PacketCode
	//------------------------------------------------------------
	BYTE							m_byPacketCode;

	//------------------------------------------------------------
	// 1byte 고정 XOR Code
	//------------------------------------------------------------
	BYTE							m_byXORCode1;
	BYTE							m_byXORCode2;

};

#endif