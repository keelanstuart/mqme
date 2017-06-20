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

#include "Packet.h"
#include <list>

class CPacketQueue
{
public:
	CPacketQueue(UINT32 initial_packet_count = 0, UINT32 initial_packet_size = 0);
	virtual ~CPacketQueue();

	CPacket *Deque(BOOL create_if_empty = false);
	void Enque(CPacket *ppkt);

	bool Empty();
	CPacket *Front();

protected:
	std::list<CPacket *> m_Que;

	UINT32 m_DefaultPacketSize;

	CRITICAL_SECTION m_Lock;
};
