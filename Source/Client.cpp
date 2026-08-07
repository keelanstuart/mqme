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
#include "Protocol.h"
#include "Socket.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>


extern pool::IThreadPool *g_ThreadPool;


class CClient : public mqme::IClient
{

public:

	virtual ~CClient()
	{
		Disconnect();
	}


	virtual void Release()
	{
		delete this;
	}


    virtual bool Connect(const char *address, uint16_t port, const channel_t *my_id)
    {
        if (!address || m_Connected)
			return false;

		addrinfo *result = nullptr;

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
		char port_string[16];
		_itoa_s(port, port_string, 10);

		if (getaddrinfo(address, port_string, &hints, &result) != 0)
			return false;

        socket_t connected_socket = invalid_socket;
        for (addrinfo *ai = result; ai != nullptr; ai = ai->ai_next)
        {
            const socket_t candidate = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (candidate == invalid_socket)
				continue;

			if (::connect(candidate, ai->ai_addr, static_cast<socket_length_t>(ai->ai_addrlen)) == 0)
            {
                connected_socket = candidate;
                break;
            }

			close_socket(candidate);
        }

		freeaddrinfo(result);

		if (connected_socket == invalid_socket)
			return false;

        m_Id = my_id ? *my_id : mqme::GenerateChannel();
        const auto hello = protocol::MakeClientHello(m_Id);
        protocol::ServerHello response{};

		if (!send_all(connected_socket, &hello, sizeof(hello)) ||
            !recv_all(connected_socket, &response, sizeof(response)) ||
            !protocol::ValidateServerHello(response))
        {
            close_socket(connected_socket);
            return false;
        }

        m_Socket = connected_socket;
        m_Connected = true;

		m_ReceiveThread = std::thread([this]
		{
			ReceiveLoop();
		});

		DispatchEvent(CONNECTED);
        return true;
    }


	virtual channel_t GetID() const
	{
		return m_Id;
	}


    virtual void Disconnect()
    {
        const bool was_connected = m_Connected.exchange(false);
        const socket_t socket = m_Socket.exchange(invalid_socket);

		shutdown_socket(socket);
        close_socket(socket);

		if (m_ReceiveThread.joinable() && m_ReceiveThread.get_id() != std::this_thread::get_id())
            m_ReceiveThread.join();

		if (was_connected)
			DispatchEvent(DISCONNECTED);
    }


    virtual bool IsConnected() const
	{
		return m_Connected;
	}


    virtual bool SendPacket(IPacket *packet)
    {
        if (!packet || !m_Connected)
			return false;

		CPacket *p = (CPacket *)packet;
        p->SetSender(m_Id);

		std::lock_guard<std::mutex> lock(m_SendMutex);

		if (!protocol::SendPacket(m_Socket, p))
        {
            Disconnect();
            return false;
        }

		return true;
    }


    virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler)
    {
        std::lock_guard<std::mutex> lock(m_HandlerMutex);

		if (handler)
			m_PacketHandlers[id] = handler;
        else
			m_PacketHandlers.erase(id);
    }


    virtual void RegisterEventHandler(EventType event, EVENT_HANDLER handler)
    {
        std::lock_guard<std::mutex> lock(m_HandlerMutex);

		if (handler)
			m_EventHandlers[event] = handler;
        else
			m_EventHandlers.erase(event);
    }


private:

	void ReceiveLoop()
    {
        while (m_Connected)
        {
            CPacket* packet = nullptr;
            const protocol::ReceiveResult result = protocol::ReceivePacket(m_Socket, packet);
            if (result == protocol::ReceiveResult::Incomplete)
                continue;

			if (result != protocol::ReceiveResult::Complete)
                break;

			PACKET_HANDLER handler;
            {
                std::lock_guard<std::mutex> lock(m_HandlerMutex);
                auto it = m_PacketHandlers.find(packet->GetID());
                if (it != m_PacketHandlers.end()) handler = it->second;
            }

			if (handler && g_ThreadPool)
            {
                packet->IncRef();

				g_ThreadPool->RunTask([this, handler, packet](size_t)
				{
                    handler(this, packet);
                    packet->Release();

					return pool::IThreadPool::TaskReturn::OK;
                });
            }

			packet->Release();
        }

		const bool was_connected = m_Connected.exchange(false);
        const socket_t socket = m_Socket.exchange(invalid_socket);
        shutdown_socket(socket);
        close_socket(socket);

		if (was_connected)
			DispatchEvent(DISCONNECTED);
    }


    void DispatchEvent(EventType event)
    {
        EVENT_HANDLER handler;
        {
            std::lock_guard<std::mutex> lock(m_HandlerMutex);

			auto it = m_EventHandlers.find(event);
            if (it != m_EventHandlers.end())
				handler = it->second;
        }

		if (handler && g_ThreadPool)
		{
			g_ThreadPool->RunTask([this, handler, event](size_t)
			{
				handler(this, event);

				return pool::IThreadPool::TaskReturn::OK;
			});
		}
    }


    std::atomic<socket_t> m_Socket{invalid_socket};
    std::atomic<bool> m_Connected{false};
    mqme::channel_t m_Id{};
    std::thread m_ReceiveThread;
    std::mutex m_SendMutex;
    std::mutex m_HandlerMutex;
    std::map<FOURCHARCODE, PACKET_HANDLER> m_PacketHandlers;
    std::map<EventType, EVENT_HANDLER> m_EventHandlers;

};


mqme::IClient *mqme::IClient::NewClient()
{
	return new CClient();
}
