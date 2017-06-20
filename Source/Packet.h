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

#include <mqme.h>

using namespace mqme;

#pragma pack(push, 1)

struct SPacketHeader
{
	FOURCHARCODE m_ID;
	GUID m_Sender;
	GUID m_Context;
	UINT32 m_DataLength;
};

#pragma pack(pop)


class CPacket : public ICorePacket
{

public:

	CPacket(UINT32 initial_size);
	virtual ~CPacket();

	virtual void Release();

	virtual void SetData(FOURCHARCODE id, UINT32 datalen, const BYTE *data);

	virtual void SetContext(GUID context);

	virtual GUID GetContext();

	virtual void SetSender(GUID id);

	virtual GUID GetSender();

	virtual FOURCHARCODE GetID();

	virtual UINT32 GetDataLength();

	virtual BYTE *GetData();

	SPacketHeader *GetHeader();

	UINT32 GetHeaderLength();

	void SetUserData(void *user);
	void *GetUserData();

	void IncRef();
	void DecRef();
	bool IsReferenced();

protected:
	void *m_Buffer;

	BYTE *m_Data;
	size_t m_AllocatedDataSize;

	void *m_UserData;

	UINT32 m_RefCt;
};
