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

#include "Packet.h"
#include "PacketQueue.h"
#include <malloc.h>

using namespace mqme;

extern CPacketQueue *g_IdlePackets;

GUID unkguid = { 0, 0, 0,{ 0, 0, 0, 0, 0, 0, 0, 0 } };

CPacket::CPacket(uint32_t initial_size)
{
	m_RefCt = 0;
	m_Data = NULL;
	m_UserData = NULL;
	m_AllocatedDataSize = initial_size;

	uint32_t datalen = initial_size + sizeof(SPacketHeader);

	m_Buffer = (initial_size > 0) ? (BYTE *)malloc(datalen) : NULL;
	if (m_Buffer)
	{
		m_Data = (BYTE *)m_Buffer + sizeof(SPacketHeader);

		memset(m_Buffer, 0, sizeof(SPacketHeader));
	}
}


CPacket::~CPacket()
{
	if (m_Buffer)
	{
		free(m_Buffer);

		m_Buffer = NULL;
		m_Data = NULL;
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

void CPacket::SetUserData(void *user)
{
	m_UserData = user;
}

void *CPacket::GetUserData()
{
	return m_UserData;
}

void CPacket::SetData(FOURCHARCODE id, uint32_t datalen, const BYTE *data)
{
	if (!m_Buffer || (m_Buffer && (m_AllocatedDataSize < datalen)))
	{
		m_Buffer = realloc(m_Buffer, datalen + sizeof(SPacketHeader));
		m_AllocatedDataSize = datalen;
		m_Data = NULL;
	}

	SPacketHeader *h = (SPacketHeader *)m_Buffer;
	if (h)
	{
		h->m_ID = id;
		h->m_DataLength = datalen;

		if (datalen)
		{
			m_Data = (BYTE *)m_Buffer + sizeof(SPacketHeader);
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


void CPacket::SetContext(GUID context)
{
	if (m_Buffer)
	{
		((SPacketHeader *)m_Buffer)->m_Context = context;
	}
}


GUID CPacket::GetContext()
{
	return m_Buffer ? ((SPacketHeader *)m_Buffer)->m_Context : unkguid;
}


void CPacket::SetSender(GUID id)
{
	if (m_Buffer)
	{
		((SPacketHeader *)m_Buffer)->m_Sender = id;
	}
}


GUID CPacket::GetSender()
{
	if (m_Buffer)
		return ((SPacketHeader *)m_Buffer)->m_Sender;

	return unkguid;
}


FOURCHARCODE CPacket::GetID()
{
	return m_Buffer ? ((SPacketHeader *)m_Buffer)->m_ID : 0;
}


uint32_t CPacket::GetHeaderLength()
{
	uint32_t ret = 0;

	SPacketHeader *h = (SPacketHeader *)m_Buffer;
	if (h)
	{
		ret = (uint32_t)sizeof(SPacketHeader);
	}

	return ret;
}


uint32_t CPacket::GetDataLength()
{
	return (m_Buffer && m_Data) ? ((SPacketHeader *)m_Buffer)->m_DataLength : 0;
}


BYTE *CPacket::GetData()
{
	return (m_Buffer && m_Data && (((SPacketHeader *)m_Buffer)->m_DataLength > 0)) ? m_Data : NULL;
}


SPacketHeader *CPacket::GetHeader()
{
	return ((SPacketHeader *)m_Buffer);
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
