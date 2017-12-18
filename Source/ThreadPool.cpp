/*
	mqme Library Source File

	Copyright © 2009-2017, Keelan Stuart. All rights reserved.

	mqme (pronounced "make me") is a Windows-only C++ API and library that facilitates easy
	distribution of network	packets	with multiple connection end-points. One-to-many is just
	as easy as one-to-one. Handling different types of incoming data is as simple as writing
	a callback that	recognizes a four character code (the 'CODE' form is the easiest way
	to use it).

	mqme was written as a response to the (IMO) confusing popularity
	of RabbitMQ, ActiveMQ, ZeroMQ, etc. RabbitMQ is written in Erlang and requires
	multiple support installations and configuration files to function -- which, I think,
	is bad for commercial products. ActiveMQ, to my knowledge, is similar. ZeroMQ forces
	the user to conform to transactional patterns that are not conducive to parallel
	processing of requests and does not allow comprehensive, complex, or numerous
	subscriptions. In essence, it was my opinion that none of those packages was
	"good enough" for me, making me write my own.

	mqme is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	mqme is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	See <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

#include <ThreadPool.h>


CThreadPool::CThreadPool(UINT threads_per_core, INT core_count_adjustment)
{
	// Find out how many cores the system has
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	// Calculate the number of threads we need
	m_nThreads = threads_per_core * max(1, (sysinfo.dwNumberOfProcessors + core_count_adjustment));

	// allocate thread handles
	m_hThreads = (HANDLE *)malloc(sizeof(HANDLE) * m_nThreads);

	InitializeCriticalSection(&m_csTaskList);

	// this is the quit semaphore...
	m_hSemaphores[TS_QUIT] = CreateSemaphore(NULL, 0, m_nThreads, NULL);

	// this is the run semaphore...
	m_hSemaphores[TS_RUN] = CreateSemaphore(NULL, 0, m_nThreads, NULL);

	for (UINT i = 0; i < m_nThreads; i++)
	{
		m_hThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_WorkerThreadProc, (LPVOID)this, 0, NULL);
	}
}


CThreadPool::~CThreadPool()
{
	PurgeAllPendingTasks();

	ReleaseSemaphore(m_hSemaphores[TS_QUIT], m_nThreads, NULL);
	WaitForMultipleObjects(m_nThreads, m_hThreads, TRUE, INFINITE);

	CloseHandle(m_hSemaphores[TS_QUIT]);
	CloseHandle(m_hSemaphores[TS_RUN]);

	free((void *)m_hThreads);

	DeleteCriticalSection(&m_csTaskList);
}



void CThreadPool::WorkerThreadProc()
{
	while (true)
	{
		DWORD waitret = WaitForMultipleObjects(TS_NUMSEMAPHORES, m_hSemaphores, false, INFINITE) - WAIT_OBJECT_0;
		if (waitret == TS_QUIT)
			break;

		STaskInfo task(nullptr, nullptr, nullptr, nullptr, nullptr);
		while (GetNextTask(task))
		{
			task.m_Task(task.m_Param[0], task.m_Param[1], task.m_Param[2]);

			if (task.m_pActionRef)
			{
				(*(task.m_pActionRef))--;
			}

			Sleep(0);
		}
	}
}


DWORD CThreadPool::_WorkerThreadProc(LPVOID param)
{
	CThreadPool *_this = (CThreadPool *)param;
	_this->WorkerThreadProc();

	return 0;
}


bool CThreadPool::RunTask(TASK_CALLBACK task, LPVOID param0, LPVOID param1, LPVOID param2, UINT numtimes, bool block)
{
	UINT blockwait;

	EnterCriticalSection(&m_csTaskList);

	for (UINT i = 0; i < numtimes; i++)
	{
		m_TaskList.push_back(STaskInfo(task, param0, param1, param2, block ? &blockwait : NULL));
	}

	LeaveCriticalSection(&m_csTaskList);

	ReleaseSemaphore(m_hSemaphores[TS_RUN], m_nThreads, NULL);

	if (block)
	{
		while (blockwait)
		{
			Sleep(1);
		}
	}

	return true;
}


void CThreadPool::WaitForAllTasks(DWORD milliseconds)
{
	while (!m_TaskList.empty())
	{
		Sleep(1);
	}
}


void CThreadPool::PurgeAllPendingTasks()
{
	EnterCriticalSection(&m_csTaskList);

	m_TaskList.clear();

	LeaveCriticalSection(&m_csTaskList);
}


UINT CThreadPool::GetNumThreads()
{
	return m_nThreads;
}


bool CThreadPool::GetNextTask(STaskInfo &task)
{
	bool ret = false;

	EnterCriticalSection(&m_csTaskList);

	if (!m_TaskList.empty())
	{
		task = m_TaskList.front();
		m_TaskList.pop_front();
		ret = true;
	}

	LeaveCriticalSection(&m_csTaskList);

	return ret;
}


CThreadPool::STaskInfo::STaskInfo(CThreadPool::TASK_CALLBACK task, LPVOID param0, LPVOID param1, LPVOID param2, UINT *pactionref) :
	m_Task(task),
	m_pActionRef(pactionref)
{
	m_Param[0] = param0;
	m_Param[1] = param1;
	m_Param[2] = param2;

	if (m_pActionRef)
	{
		(*m_pActionRef)++;
	}
}