/*
	mqme Library Source File

	Copyright © 2009-2021, Keelan Stuart. All rights reserved.

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

#include "PacketQueue.h"

CPacketQueue::CPacketQueue(uint32_t initial_packet_count, uint32_t initial_packet_size)
{
	InitializeCriticalSection(&m_Lock);

	m_DefaultPacketSize = initial_packet_size;
	while (initial_packet_count)
	{
		CPacket *ppkt = new CPacket(initial_packet_size);

		if (ppkt)
		{
			Enque(ppkt);
		}
		else
		{
			break;
		}

		initial_packet_count--;
	}
}


CPacketQueue::~CPacketQueue()
{
	while (!m_Que.empty())
	{
		CPacket *ppkt = (CPacket *)(m_Que.front());
		if (ppkt)
		{
			delete ppkt;
		}
		m_Que.pop_front();
	}

	DeleteCriticalSection(&m_Lock);
}


CPacket *CPacketQueue::Deque(BOOL create_if_empty)
{
	CPacket *ret = NULL;

	EnterCriticalSection(&m_Lock);

	if (m_Que.empty())
	{
		ret = create_if_empty ? new CPacket(m_DefaultPacketSize) : NULL;
	}
	else
	{
		ret = m_Que.front();
		m_Que.pop_front();
	}

	LeaveCriticalSection(&m_Lock);

	return ret;
}


void CPacketQueue::Enque(CPacket *ppkt)
{
	EnterCriticalSection(&m_Lock);

	m_Que.push_back(ppkt);

	LeaveCriticalSection(&m_Lock);
}


bool CPacketQueue::Empty()
{
	return m_Que.empty();
}


CPacket *CPacketQueue::Front()
{
	return m_Que.front();
}
