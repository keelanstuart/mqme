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

CPacketQueue::CPacketQueue(size_t initial_packet_count, uint32_t initial_packet_size)
{
	m_DefaultPacketSize = initial_packet_size;
	while (initial_packet_count)
	{
		CPacket *ppkt = new CPacket(initial_packet_size);
		m_Queue.push(ppkt);

		initial_packet_count--;
	}
}


CPacketQueue::~CPacketQueue()
{
	// delete all packets
	while (!m_Queue.empty())
	{
		CPacket *ppkt = m_Queue.back();
		delete ppkt;
		m_Queue.pop();
	}
}


CPacket *CPacketQueue::Deque(bool create_if_empty)
{
	std::lock_guard<std::mutex> l(m_Lock);

	// if the queue isn't empty, pop the front and return it
	if (!m_Queue.empty())
	{
		CPacket *ret = m_Queue.front();
		m_Queue.pop();
		return ret;
	}

	// the queue was empty... so either make a new packet and return it or return a nullptr
	return (create_if_empty ? new CPacket(m_DefaultPacketSize) : nullptr);
}


void CPacketQueue::Enque(CPacket *ppkt)
{
	std::lock_guard<std::mutex> l(m_Lock);

	m_Queue.push(ppkt);
}


bool CPacketQueue::Empty()
{
	return m_Queue.empty();
}
