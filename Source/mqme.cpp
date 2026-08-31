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

#include <mqme.h>
#include <Pool.h>
#include "Packet.h"
#include "PacketQueue.h"
#include "Socket.h"

#include <atomic>
#include <mutex>
#include <random>

#if defined(_WIN32)

#include <objbase.h>

#pragma comment(lib, "Ws2_32.lib")

#else

#include <uuid/uuid.h>

#endif

CPacketQueue* g_IdlePackets = nullptr;
pool::IThreadPool* g_ThreadPool = nullptr;
std::mutex g_InitMutex;
bool g_Initialized = false;


bool mqme::Initialize(size_t packet_count, size_t packet_size,
                      size_t threads_per_core, int core_adjustment)
{
    std::lock_guard<std::mutex> lock(g_InitMutex);

    if (g_Initialized)
		return true;

    if (!socket_platform_initialize())
		return false;

    g_IdlePackets = new CPacketQueue(packet_count, packet_size);

    g_ThreadPool = pool::IThreadPool::Create(threads_per_core, core_adjustment);

    g_Initialized = g_IdlePackets && g_ThreadPool;
    if (!g_Initialized)
    {
        if (g_ThreadPool)
		{
			g_ThreadPool->Release();
			g_ThreadPool = nullptr;
		}

        delete g_IdlePackets;
		g_IdlePackets = nullptr;

        socket_platform_close();
    }

    return g_Initialized;
}

void mqme::Close()
{
    std::lock_guard<std::mutex> lock(g_InitMutex);

    if (g_ThreadPool)
    {
        g_ThreadPool->WaitForAllTasks(uint32_t(-1));
        g_ThreadPool->Release();
        g_ThreadPool = nullptr;
    }

    delete g_IdlePackets;
    g_IdlePackets = nullptr;

    if (g_Initialized)
		socket_platform_close();

    g_Initialized = false;
}


mqme::channel_t mqme::GenerateChannel()
{
    channel_t result;

#if defined(_WIN32)

    CoCreateGuid(&result.m_Guid);

#else

    uuid_generate(result.m_GuidBytes);

#endif

    return result;
}


channel_t mqme::NullChannel()
{
    static channel_t null_channel = { 0 };

    return null_channel;
}


mqme::IPacket* mqme::IPacket::NewPacket()
{
	CPacket *pkt = g_IdlePackets ? g_IdlePackets->Deque(true) : nullptr;

	channel_t g = { 0 };
	pkt->SetContext(g);
	pkt->IncRef();

	return pkt;
}
