#pragma once
#include "MemoryPoolTLS.h"

template <class T>
class CLockFreeStack
{
public:
	CLockFreeStack();
	~CLockFreeStack();
	struct Node
	{
		T _data;
		Node* _next;
	};
	void Push(T data);
	bool Pop(T* pData);
private:
	struct alignas(16) UNIQUE_TOP
	{
		Node* _top;
		UINT64 _uniqIdx;
	};
	volatile UNIQUE_TOP _uniqTop;
	Node _dummyNode;

	static MemoryPoolTLS<Node> poolManager;
};

template <class T>
MemoryPoolTLS<typename CLockFreeStack<T>::Node> CLockFreeStack<T>::poolManager(10000, POOL_MAX_ALLOC, false);

template <class T>
CLockFreeStack<T>::CLockFreeStack()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if ((__int64)si.lpMaximumApplicationAddress != 0x00007FFFFFFEFFFF)
	{
		wprintf(L"It does not work with the current version of the current OS.\n");
		TerminateProcess(GetCurrentProcess(), 1);
	}
	_dummyNode._next = nullptr;
	_uniqTop._top = &_dummyNode;
	_uniqTop._uniqIdx = 0;
}

template <class T>
CLockFreeStack<T>::~CLockFreeStack()
{
	T popedData;
	while (Pop(&popedData));
}

template <class T>
void CLockFreeStack<T>::Push(T data)
{
	Node* bkTop;
	Node* pNewnode = poolManager.Alloc();
	pNewnode->_data = data;
	do
	{
		bkTop = _uniqTop._top;
		pNewnode->_next = bkTop;
	} while (InterlockedCompareExchange64((PLONG64)&_uniqTop._top, (LONG64)pNewnode, (LONG64)bkTop) != (LONG64)bkTop);
}

template <class T>
bool CLockFreeStack<T>::Pop(T* pData)
{
	Node* pNextTop;
	UNIQUE_TOP popNode;
	do
	{
		popNode._uniqIdx = _uniqTop._uniqIdx;
		popNode._top = _uniqTop._top;
		pNextTop = popNode._top->_next;

		if (pNextTop == nullptr)
		{
			*pData = NULL;
			return false;
		}

	} while (!InterlockedCompareExchange128((PLONG64)&_uniqTop, (LONG64)(popNode._uniqIdx + 1), (LONG64)pNextTop, (PLONG64)&popNode));

	*pData = popNode._top->_data;
	poolManager.Free(popNode._top);
	return true;
}