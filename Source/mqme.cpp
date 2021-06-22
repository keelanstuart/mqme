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

#include <mqme.h>
#include "Packet.h"
#include "PacketQueue.h"
#include <Pool.h>


// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")



using namespace mqme;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;

		case DLL_PROCESS_DETACH:
			break;
	}

	return TRUE;
}

CPacketQueue *g_IdlePackets = NULL;
pool::IThreadPool *g_ThreadPool = NULL;
bool g_Initialized = false;

bool operator <(const GUID &a, const GUID &b) { return (memcmp(&a, &b, sizeof(GUID)) <= 0) ? true : false; }

bool mqme::Initialize(UINT initial_idle_packet_count, UINT initial_packet_size, UINT threads_per_core, INT core_count_adjustment)
{
	if (g_Initialized)
		return true;

	WSADATA wsaData;
    if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
        return false;

	g_IdlePackets = new CPacketQueue(initial_idle_packet_count, initial_packet_size);
	g_ThreadPool = pool::IThreadPool::Create(threads_per_core, core_count_adjustment);

	g_Initialized = (g_IdlePackets != nullptr) && (g_ThreadPool != nullptr);

	return g_Initialized;
}

void mqme::Close()
{
	if (g_Initialized)
	{
		WSACleanup();
		g_Initialized = false;
	}

	if (g_IdlePackets)
	{
		delete g_IdlePackets;
		g_IdlePackets = NULL;
	}

	if (g_ThreadPool)
	{
		g_ThreadPool->Release();
		g_ThreadPool = NULL;
	}
}

ICorePacket *ICorePacket::NewPacket()
{
	CPacket *pkt = g_IdlePackets ? g_IdlePackets->Deque(true) : NULL;

	GUID g = { 0 };
	pkt->SetContext(g);

	return pkt;
}
