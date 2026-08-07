/*
	mqme Library Source File

	Copyright © 2009-2026, Keelan Stuart. All rights reserved.

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

#include "Packet.h"
#include "PacketQueue.h"
#include <malloc.h>

using namespace mqme;

extern CPacketQueue *g_IdlePackets;



CPacket::CPacket(size_t initial_size)
{
	m_RefCt = 0;
	m_Data = nullptr;
	m_AllocatedDataSize = initial_size;

	size_t datalen = initial_size + sizeof(SPacketHeader);

	m_Buffer = (initial_size > 0) ? (uint8_t *)malloc(datalen) : nullptr;
	if (m_Buffer)
	{
		m_Data = (uint8_t *)m_Buffer + sizeof(SPacketHeader);

		memset(m_Buffer, 0, sizeof(SPacketHeader));

		((SPacketHeader *)m_Buffer)->m_HeaderLength = sizeof(SPacketHeader);
	}
}


CPacket::~CPacket()
{
	if (m_Buffer)
	{
		free(m_Buffer);

		m_Buffer = nullptr;
		m_Data = nullptr;
		m_AllocatedDataSize = 0;
	}
}


void CPacket::Release()
{
	// if somebody tries to multi-release a packet after it's in the idle queue, 
	// don't re-add it to the idle queue - because it's already there! party foul!
	bool isref = IsReferenced();
	DecRef();

	if (isref && !IsReferenced())
	{
		if (g_IdlePackets)
		{
			g_IdlePackets->Enque(this);
		}
	}
}


void CPacket::SetData(FOURCHARCODE id, size_t datalen, const void *data)
{
	if (!m_Buffer || (m_Buffer && (m_AllocatedDataSize < datalen)))
	{
		void *temp = realloc(m_Buffer, datalen + sizeof(SPacketHeader));
		if (!temp)
			throw;

		m_Buffer = temp;
		m_AllocatedDataSize = datalen;
		m_Data = nullptr;
	}

	SPacketHeader *h = (SPacketHeader *)m_Buffer;
	if (h)
	{
		h->m_ID = id;
		h->m_DataLength = (uint32_t)datalen;

		if (datalen)
		{
			m_Data = (uint8_t *)m_Buffer + sizeof(SPacketHeader);
		}
	}

	if (m_Data)
	{
		if (data)
		{
			memcpy(m_Data, data, datalen);
		}
	}
}


void CPacket::SetContext(channel_t context)
{
	if (m_Buffer)
	{
		((SPacketHeader *)m_Buffer)->m_Context = context;
	}
}


channel_t CPacket::GetContext() const
{
	return ((SPacketHeader *)m_Buffer)->m_Context;
}


void CPacket::SetSender(channel_t id)
{
	((SPacketHeader *)m_Buffer)->m_Sender = id;
}


channel_t CPacket::GetSender() const
{
	return ((SPacketHeader *)m_Buffer)->m_Sender;
}


FOURCHARCODE CPacket::GetID() const
{
	return m_Buffer ? ((SPacketHeader *)m_Buffer)->m_ID : 0;
}


size_t CPacket::GetDataLength() const
{
	return ((SPacketHeader *)m_Buffer)->m_DataLength;
}


const uint8_t *CPacket::GetData() const
{
	return (((SPacketHeader *)m_Buffer)->m_DataLength > 0) ? m_Data : nullptr;
}


const SPacketHeader *CPacket::GetHeader() const
{
	return ((SPacketHeader *)m_Buffer);
}


size_t CPacket::GetFrameLength() const
{
	return ((SPacketHeader *)m_Buffer)->m_HeaderLength + ((SPacketHeader *)m_Buffer)->m_DataLength;
}


void CPacket::IncRef()
{
	m_RefCt++;
}


void CPacket::DecRef()
{
	if (m_RefCt)
		m_RefCt--;
}

bool CPacket::IsReferenced()
{
	return (m_RefCt != 0);
}
