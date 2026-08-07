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

#pragma once

#include <mqme.h>

using namespace mqme;

#pragma pack(push, 1)

struct SPacketHeader
{
	FOURCHARCODE m_ID;
	uint32_t m_HeaderLength;
	uint32_t m_DataLength;
	channel_t m_Sender;
	channel_t m_Context;
	uint32_t m_Flags;

	inline void ToNetwork()
	{
		m_ID = htonl(m_ID);
		m_HeaderLength = htonl(m_HeaderLength);
		m_DataLength = htonl(m_DataLength);
		m_Flags = htonl(m_Flags);
	}

	inline void ToHost()
	{
		m_ID = ntohl(m_ID);
		m_HeaderLength = ntohl(m_HeaderLength);
		m_DataLength = ntohl(m_DataLength);
		m_Flags = ntohl(m_Flags);
	}
};

#pragma pack(pop)


class CPacket : public IPacket
{

public:

	CPacket(size_t initial_size);
	virtual ~CPacket();

	virtual void Release();

	virtual void SetData(FOURCHARCODE id, size_t datalen, const void *data);

	virtual void SetContext(channel_t context);

	virtual channel_t GetContext() const;

	void SetSender(channel_t id);

	virtual channel_t GetSender() const;

	virtual FOURCHARCODE GetID() const;

	virtual size_t GetDataLength() const;

	virtual const uint8_t *GetData() const;

	const SPacketHeader *GetHeader() const;

	size_t GetFrameLength() const;

	void IncRef();
	void DecRef();
	bool IsReferenced();

protected:
	void *m_Buffer;

	BYTE *m_Data;
	size_t m_AllocatedDataSize;

	uint32_t m_RefCt;
};
