/*
mqme Library Source File

Copyright © 2009-2017, Keelan Stuart. All rights reserved.

mqme is a Windows-only C++ API and library that facilitates easy distribution of network
packets	with multiple connection end-points. One-to-many is just as easy as one-to-one.
Handling different types of incoming data is as simple as writing a callback that
recognizes a four character code (the 'CODE' form is the easiest way to use it).

mqme (pronounced "make me") was written as a response to the (IMO) confusing popularity
of RabbitMQ, ActiveMQ, ZeroMQ, etc. RabbitMQ is written in Erlang and requires
multiple support installations and configuration files to function -- which is bad for
commercial products. ActiveMQ, to my knowledge, is similar. ZeroMQ forces the user
to conform to transactional patterns that are not conducive to parallel processing
of requests and does not allow comprehensive, complex, or numerous subscriptions.
In essence, it was my opinion that none of those packages was "good enough" for me,
making me write my own.

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

#pragma once


class CThreadPool
{
public:
	typedef void (WINAPI *TASK_CALLBACK)(LPVOID param0, LPVOID param1, LPVOID param2);

	// The number of threads created will be:
	//   threads_per_core * max(1, (core_count + core_count_adjustment))
	CThreadPool(UINT threads_per_core, INT core_count_adjustment);

	virtual ~CThreadPool();

	// the number of worker threads in the pool
	UINT GetNumThreads();

	bool RunTask(TASK_CALLBACK func, LPVOID param0 = nullptr, LPVOID param1 = nullptr, LPVOID param2 = nullptr, UINT numtimes = 1, bool block = false);

	void WaitForAllTasks(DWORD milliseconds);

	void PurgeAllPendingTasks();

protected:

	struct STaskInfo
	{
		STaskInfo(TASK_CALLBACK task, LPVOID param0, LPVOID param1, LPVOID param2, UINT *pactionref);

		// The function that the thread should be running
		TASK_CALLBACK m_Task;

		// The parameter given to the thread function
		LPVOID m_Param[3];

		// This is the number of active tasks, used for blocking
		UINT *m_pActionRef;
	};

	typedef std::deque<STaskInfo> TTaskList;

	TTaskList m_TaskList;

	CRITICAL_SECTION m_csTaskList;

	bool GetNextTask(STaskInfo &task);

	void WorkerThreadProc();
	static DWORD WINAPI _WorkerThreadProc(LPVOID param);

	// the number of threads in the pool
	UINT m_nThreads;

	// the actual thread handles... keep them separated from SThreadInfo
	// so we can wait on them.
	HANDLE *m_hThreads;

	enum
	{
		TS_QUIT = 0,		// indicates it's time for a thread to shut down
		TS_RUN,				// indicates that the thread should start running the function given

		TS_NUMSEMAPHORES
	};

	HANDLE m_hSemaphores[TS_NUMSEMAPHORES];

};
