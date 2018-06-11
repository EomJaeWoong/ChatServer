#ifndef __LOCKFREEQUEUE__H__
#define __LOCKFREEQUEUE__H__


/*--------------------------------------------------------------------------*/
// Lockfree Queue
/*--------------------------------------------------------------------------*/
template <class DATA>
class CLockfreeQueue
{
private :
	/*----------------------------------------------------------------------*/
	// Lockfree Queue의 노드 구조체
	/*----------------------------------------------------------------------*/
	struct st_NODE
	{
		DATA		Data;
		st_NODE		*pNext;
	};

	/*----------------------------------------------------------------------*/
	// Lockfree Queue의 Head, Tail 노드 구조체
	/*----------------------------------------------------------------------*/
	struct st_END_NODE
	{
		st_NODE		*pEndNode;
		__int64		iUniqueNum;
	};




public :
	//////////////////////////////////////////////////////////////////////////
	// 생성자
	//////////////////////////////////////////////////////////////////////////
				CLockfreeQueue()
	{
		_lUseSize			= 0;

		_iUniqueNumHead		= 0;
		_iUniqueNumTail		= 0;

		_pQueuePool			= new CMemoryPool<st_NODE>;

		//////////////////////////////////////////////////////////////////////
		// 더미 노드
		//////////////////////////////////////////////////////////////////////
		st_NODE *pDummyNode	= _pQueuePool->Alloc();
		pDummyNode->pNext	= nullptr;

		_pHead				= (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pHead->pEndNode	= pDummyNode;
		_pHead->iUniqueNum	= 0;

		_pTail				= (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pTail->pEndNode	= pDummyNode;
		_pTail->iUniqueNum	= 0;
	}


	//////////////////////////////////////////////////////////////////////////
	// 소멸자
	//////////////////////////////////////////////////////////////////////////
	virtual		~CLockfreeQueue()
	{
		st_NODE *pDeleteNode;

		while (_pHead->pEndNode != nullptr)
		{
			pDeleteNode			= _pHead->pEndNode;
			_pHead->pEndNode	= _pHead->pEndNode->pNext;
			_pQueuePool->Free(pDeleteNode);
		}

		delete _pQueuePool;

		_aligned_free(_pHead);
		_aligned_free(_pTail);
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue 비워주기
	//////////////////////////////////////////////////////////////////////////
	void		ClearBuffer(void)
	{
		st_NODE *pDeleteNode;

		while (!isEmpty())
		{
			pDeleteNode = _pHead->pEndNode;
			_pHead->pEndNode = _pHead->pEndNode->pNext;
			_pQueuePool->Free(pDeleteNode);

			_lUseSize--;
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue 데이터 삽입
	//////////////////////////////////////////////////////////////////////////
	bool		Put(DATA Data)
	{
		st_END_NODE		stCloneTailNode;
		st_NODE			*pNextTail;

		__int64			iUniqueNumTail;

		//////////////////////////////////////////////////////////////////////
		// 데이터 노드 생성
		//////////////////////////////////////////////////////////////////////
		st_NODE			*pNode = _pQueuePool->Alloc();
		pNode->Data = Data;
		pNode->pNext = nullptr;

		while (1)
		{
			iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);

			//////////////////////////////////////////////////////////////////
			// Tail 저장
			//////////////////////////////////////////////////////////////////
			stCloneTailNode.iUniqueNum = _pTail->iUniqueNum;
			stCloneTailNode.pEndNode = _pTail->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Tail의 Next 저장
			//////////////////////////////////////////////////////////////////
			pNextTail = stCloneTailNode.pEndNode->pNext;

			//////////////////////////////////////////////////////////////////
			// Tail 다음이 NULL이면(Tail 다음값을 비교해서)
			// Node를 붙이고
			// Tail을 밀어준다 
			//
			// Tail이 밀리지 않아도 다른 곳(다른 쓰레드의 Put, Get에서 밀어준다)
			//////////////////////////////////////////////////////////////////
			if (nullptr == pNextTail)
			{
				if (nullptr == InterlockedCompareExchangePointer(
					(PVOID *)&stCloneTailNode.pEndNode->pNext,
					pNode,
					pNextTail
					))
				{
					InterlockedCompareExchange128(
						(LONG64 *)_pTail,
						iUniqueNumTail,
						(LONG64)stCloneTailNode.pEndNode->pNext,
						(LONG64 *)&stCloneTailNode
						);
					break;
				}
			}
		

			//////////////////////////////////////////////////////////////////
			// next가 있으면 일단 밀어줌
			// 
			// Tail이 바뀌었기 때문에 UniqueNum을 올려줌
			//////////////////////////////////////////////////////////////////
			else
			{
				InterlockedCompareExchange128(
					(LONG64 *)_pTail,
					iUniqueNumTail,
					(LONG64)stCloneTailNode.pEndNode->pNext,
					(LONG64 *)&stCloneTailNode
					);

				//iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);
			}
		}

		InterlockedIncrement((long *)&_lUseSize);

		return true;
	}



	//////////////////////////////////////////////////////////////////////////
	// Queue 데이터 추출
	//////////////////////////////////////////////////////////////////////////
	bool		Get(DATA *pOutData)
	{
		st_END_NODE		stCloneHeadNode, stCloneTailNode;
		st_NODE			*pNextHead;

		__int64			iUniqueNumHead = InterlockedIncrement64((LONG64 *)&_iUniqueNumHead);

		InterlockedDecrement((long *)&_lUseSize);	
		
		while (1)
		{
			//////////////////////////////////////////////////////////////////
			// Head 저장
			//////////////////////////////////////////////////////////////////
			stCloneHeadNode.iUniqueNum = _pHead->iUniqueNum;
			stCloneHeadNode.pEndNode = _pHead->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Tail 저장
			//////////////////////////////////////////////////////////////////
			stCloneTailNode.iUniqueNum = _pTail->iUniqueNum;
			stCloneTailNode.pEndNode = _pTail->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Head의 Next 저장
			//////////////////////////////////////////////////////////////////
			pNextHead = stCloneHeadNode.pEndNode->pNext;

			//////////////////////////////////////////////////////////////////
			// 비었는지 확인
			//////////////////////////////////////////////////////////////////
			if (isEmpty())
			{
				pOutData = nullptr;
				return false;
			}

			//////////////////////////////////////////////////////////////////
			// tail뒤에 Node가 있으면 밀어주기
			//////////////////////////////////////////////////////////////////
			else if (nullptr != (stCloneTailNode.pEndNode->pNext))
			{
				__int64 iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);
				InterlockedCompareExchange128(
					(LONG64 *)_pTail,
					iUniqueNumTail,
					(LONG64)stCloneTailNode.pEndNode->pNext,
					(LONG64 *)&stCloneTailNode
					);
			}

			//////////////////////////////////////////////////////////////////
			// Node가 있을 경우
			// Head값을 비교하여 바꿔줌
			//
			// 노드가 없다고 나오는데 
			// 이 구간에서 밑의 경우가 있으니 체크해 줌
			// (Node가 있지만 비어있다고 나오는 상태)
			//  ---      ---
			// |   | -> |   |
			//  ---      ---
			//  ^ ^
			//  H T
			//////////////////////////////////////////////////////////////////
			else
			{
				if (nullptr != pNextHead)
				{
					*pOutData = pNextHead->Data;
					if (InterlockedCompareExchange128(
						(LONG64 *)_pHead,
						iUniqueNumHead,
						(LONG64)stCloneHeadNode.pEndNode->pNext,
						(LONG64 *)&stCloneHeadNode
						))
					{
						_pQueuePool->Free(stCloneHeadNode.pEndNode);
						break;
					}
				}
			}
		}
		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue가 비었는지 확인
	//////////////////////////////////////////////////////////////////////////
	bool		isEmpty(void)
	{
		if (0 == _lUseSize)
		{
			if ((_pHead->pEndNode) == (_pTail->pEndNode))
				return true;
		}

		return false;
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue 사용량
	//////////////////////////////////////////////////////////////////////////
	long		GetUseSize(void) {	return _lUseSize;		}


	//////////////////////////////////////////////////////////////////////////
	// Queue의 메모리풀 사용량
	//////////////////////////////////////////////////////////////////////////
	long		GetAllocSize(void) { return _pQueuePool->GetAllocCount(); }
	




private :
	//////////////////////////////////////////////////////////////////////////
	// Queue의 메모리 풀
	//////////////////////////////////////////////////////////////////////////
	CMemoryPool<st_NODE>	*_pQueuePool;


	//////////////////////////////////////////////////////////////////////////
	// Queue의 Head, Tail
	//////////////////////////////////////////////////////////////////////////
	st_END_NODE				*_pHead;
	st_END_NODE				*_pTail;


	//////////////////////////////////////////////////////////////////////////
	// Head, Tail의 유니크 넘버
	//////////////////////////////////////////////////////////////////////////
	__int64					_iUniqueNumHead;
	__int64					_iUniqueNumTail;


	//////////////////////////////////////////////////////////////////////////
	// Queue 사용량
	//////////////////////////////////////////////////////////////////////////
	long					_lUseSize;
};

#endif