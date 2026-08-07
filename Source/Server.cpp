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
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>


extern pool::IThreadPool* g_ThreadPool;


class ChannelSet : public IChannelSet
{

public:
	virtual void Release()
	{}

	virtual void Add(mqme::channel_t id)
	{
		m_Channels.insert(id);
	}

	virtual void Remove(mqme::channel_t id)
	{
		m_Channels.erase(id);
	}

	virtual bool Contains(mqme::channel_t id) const
	{
		return m_Channels.find(id) != m_Channels.end();
	}

	virtual size_t Size() const
	{
		return m_Channels.size();
	}

	virtual bool Empty() const
	{
		return m_Channels.empty();
	}

	virtual void ForEach(PerChannelFunc func) const
	{
		if (!func)
			return;

		for (const channel_t &value : m_Channels)
			func(value);
	}

private:
	std::set<channel_t> m_Channels;

};


struct SConnection
{
    mqme::channel_t id{};
    socket_t socket = invalid_socket;
    std::mutex send_mutex;
    std::atomic<bool> receive_pending{false};
    std::atomic<bool> disconnected{false};
    SConnection *retired_next = nullptr;
};


class CServer final : public mqme::IServer
{

public:

	CServer()
	{
	}

	
	virtual ~CServer()
	{
		StopListening();
		DeleteConnections();
	}

	
	virtual void Release()
	{
		delete this;
	}


	virtual bool StartListening(uint16_t port)
    {
        if (m_Running)
			return true;

        socket_t listener = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (listener == invalid_socket)
			return false;

        int yes = 1;
        int no = 0;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
        setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&no, sizeof(no));

        sockaddr_in6 address{};
        address.sin6_family = AF_INET6;
        address.sin6_addr = in6addr_any;
        address.sin6_port = htons(port);

		if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listener, SOMAXCONN) != 0)
        {
            close_socket(listener);
            return false;
        }

        m_ListenSocket = listener;
        m_Running = true;
        m_AcceptThread = std::thread([this] { AcceptLoop(); });
        m_ReceiveThread = std::thread([this] { ReceiveLoop(); });

		return true;
    }


    virtual bool StopListening()
    {
        if (!m_Running.exchange(false))
			return true;

        const socket_t listener = m_ListenSocket.exchange(invalid_socket);
        shutdown_socket(listener);
        close_socket(listener);

		m_ConnectionCondition.notify_all();

        if (m_AcceptThread.joinable())
			m_AcceptThread.join();

		if (m_ReceiveThread.joinable())
			m_ReceiveThread.join();

        {
            std::lock_guard<std::mutex> lock(m_ConnectionMutex);

			for (auto &entry : m_Connections)
            {
                SConnection* conn = entry.second;
                conn->disconnected = true;

				std::lock_guard<std::mutex> send_lock(conn->send_mutex);

				shutdown_socket(conn->socket);
            }
        }

        {
            std::unique_lock<std::mutex> lock(m_TaskMutex);

			m_TaskCondition.wait(lock, [this]{ return m_ActiveTasks == 0; });
        }

        {
            std::lock_guard<std::mutex> lock(m_ConnectionMutex);

			for (auto& entry : m_Connections)
            {
                SConnection *conn = entry.second;

				std::lock_guard<std::mutex> send_lock(conn->send_mutex);

				close_socket(conn->socket);
                conn->socket = invalid_socket;
                conn->retired_next = m_Retired;
                m_Retired = conn;
            }

			m_Connections.clear();
        }

        {
            std::lock_guard<std::mutex> lock(m_RoutingMutex);

			m_RoutingMap.clear();
            m_ListeningMap.clear();
        }

		return true;
    }


	virtual bool SendPacket(IPacket *packet)
    {
        if (!packet)
			return false;

		return RoutePacket((CPacket *)packet, NullChannel());
    }


	virtual bool AddListenerToChannel(channel_t channel, channel_t listener)
    {
        {
            std::lock_guard<std::mutex> connection_lock(m_ConnectionMutex);

			if (m_Connections.find(listener) == m_Connections.end())
				return false;
        }

        std::lock_guard<std::mutex> routing_lock(m_RoutingMutex);

		m_RoutingMap[channel].Add(listener);
        m_ListeningMap[listener].Add(channel);

		return true;
    }


    virtual void RemoveListenerFromChannel(channel_t channel, channel_t listener)
    {
        std::lock_guard<std::mutex> lock(m_RoutingMutex);

		auto route = m_RoutingMap.find(channel);
        if (route != m_RoutingMap.end())
        {
            route->second.Remove(listener);
            if (route->second.Empty())
				m_RoutingMap.erase(route);
        }

		auto listening = m_ListeningMap.find(listener);
        if (listening != m_ListeningMap.end())
        {
            listening->second.Remove(channel);
            if (listening->second.Empty())
				m_ListeningMap.erase(listening);
        }
    }


    virtual bool GetListeners(channel_t channel, IChannelSet **listeners)
    {
        if (!listeners)
			return false;

		std::lock_guard<std::mutex> lock(m_RoutingMutex);

		auto route = m_RoutingMap.find(channel);
        if (route == m_RoutingMap.end())
        {
            *listeners = nullptr;
            return false;
        }

		*listeners = &route->second;

		return true;
    }


    virtual void RegisterPacketHandler(mqme::FOURCHARCODE id, PACKET_HANDLER handler)
    {
        std::lock_guard<std::mutex> lock(m_HandlerMutex);

		if (handler)
			m_PacketHandlers[id] = handler;
        else
			m_PacketHandlers.erase(id);
    }


	void RegisterEventHandler(EventType event, EVENT_HANDLER handler)
    {
        std::lock_guard<std::mutex> lock(m_HandlerMutex);

		if (handler)
			m_EventHandlers[event] = handler;
        else
			m_EventHandlers.erase(event);
    }


private:

	void BeginTask()
    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);

		m_ActiveTasks++;
    }

    void EndTask()
    {
        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);

			m_ActiveTasks--;
        }

		m_TaskCondition.notify_all();
    }


	void SetHandshakeTimeout(socket_t socket)
    {

#if defined(_WIN32)

		DWORD timeout = 5000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

#else

		timeval timeout{};
        timeout.tv_sec = 5;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

#endif

	}


	void ClearHandshakeTimeout(socket_t socket)
    {

#if defined(_WIN32)

		DWORD timeout = 0;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

#else

		timeval timeout{};
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

#endif

	}


	void AcceptLoop()
    {
        while (m_Running)
        {
            sockaddr_storage address{};
            socket_length_t size = sizeof(address);

			socket_t socket = ::accept(m_ListenSocket, reinterpret_cast<sockaddr*>(&address), &size);
            if (socket == invalid_socket)
            {
                if (!m_Running)
					break;

				continue;
            }

            SetHandshakeTimeout(socket);

			protocol::ClientHello hello{};
            if (!recv_all(socket, &hello, sizeof(hello)) || !protocol::ValidateClientHello(hello))
            {
                close_socket(socket);
                continue;
            }

			ClearHandshakeTimeout(socket);

            uint32_t features = ntohl(hello.features) & protocol::supported_features;

			SConnection *conn = new ::SConnection();
            conn->id = hello.client_id;
            conn->socket = socket;

            bool inserted = false;
            {
                std::lock_guard<std::mutex> lock(m_ConnectionMutex);

				inserted = m_Connections.insert({conn->id, conn}).second;
            }

			if (!inserted)
            {
                const protocol::ServerHello rejection = protocol::MakeServerHello(2, NullChannel(), features);
				send_all(socket, &rejection, sizeof(rejection));

				close_socket(socket);

				delete conn;

				continue;
            }

            const protocol::ServerHello response = protocol::MakeServerHello(0, NullChannel(), features);
            if (!send_all(socket, &response, sizeof(response)))
            {
                DisconnectConnection(conn, false);

				continue;
            }

            AddListenerToChannel(conn->id, conn->id);
            m_ConnectionCondition.notify_all();
            DispatchEvent(CONNECT, conn->id);
        }
    }

    void ReceiveLoop()
    {
        while (m_Running)
        {
            m_PollConnections.clear();
            m_PollDescriptors.clear();

            {
                std::unique_lock<std::mutex> lock(m_ConnectionMutex);

				if (m_Connections.empty())
                {
                    m_ConnectionCondition.wait(lock, [this]
					{
						return !m_Running || !m_Connections.empty();
					});

					if (!m_Running)
						break;
                }

                m_PollConnections.reserve(m_Connections.size());
                m_PollDescriptors.reserve(m_Connections.size());

				for (const auto& entry : m_Connections)
                {
                    SConnection* conn = entry.second;

					if (conn->disconnected)
						continue;

					pollfd_t descriptor{};
                    descriptor.fd = conn->socket;
                    descriptor.events = POLLIN;

					m_PollConnections.push_back(conn);
                    m_PollDescriptors.push_back(descriptor);
                }
            }

            if (m_PollDescriptors.empty())
				continue;

			int ready = poll_descriptors(m_PollDescriptors.data(), m_PollDescriptors.size(), 100);
            if (ready <= 0)
				continue;

            for (size_t i = 0; i < m_PollDescriptors.size(); i++)
            {
                short events = m_PollDescriptors[i].revents;
				if (!events)
					continue;

				SConnection* conn = m_PollConnections[i];

                if ((events & POLLIN) && !conn->receive_pending.exchange(true))
                {
                    BeginTask();

					g_ThreadPool->RunTask([this, conn](std::size_t)
                    {
                        ReceiveConnection(conn);
                        EndTask();

						return pool::IThreadPool::TaskReturn::OK;
                    });
                }
                else if (events & (POLLERR | POLLHUP | POLLNVAL))
                {
                    DisconnectConnection(conn, true);
                }
            }
        }
    }

    void ReceiveConnection(SConnection* conn)
    {
        CPacket* packet = nullptr;

		protocol::ReceiveResult result = protocol::ReceivePacket(conn->socket, packet);
        conn->receive_pending = false;

        if (result == protocol::ReceiveResult::Incomplete)
			return;

		if (result != protocol::ReceiveResult::Complete)
        {
            DisconnectConnection(conn, true);

			return;
        }

        packet->SetSender(conn->id);

		PACKET_HANDLER handler{};
        {
            std::lock_guard<std::mutex> lock(m_HandlerMutex);

			auto found = m_PacketHandlers.find(packet->GetID());
            if (found != m_PacketHandlers.end())
				handler = found->second;
        }

        if (handler)
        {
            packet->IncRef();
            BeginTask();

			g_ThreadPool->RunTask([this, handler, packet](size_t)
            {
                handler(this, packet);
                packet->Release();
                EndTask();

				return pool::IThreadPool::TaskReturn::OK;
            });
        }

        RoutePacket(packet, conn->id);
        packet->Release();
    }


    void DisconnectConnection(SConnection* conn, bool dispatch)
    {
        if (!conn || conn->disconnected.exchange(true))
			return;

        {
            std::lock_guard<std::mutex> routing_lock(m_RoutingMutex);

			auto listening = m_ListeningMap.find(conn->id);
            if (listening != m_ListeningMap.end())
            {
                listening->second.ForEach([this, conn](channel_t channel)
                {
                    auto route = m_RoutingMap.find(channel);
                    if (route != m_RoutingMap.end())
                    {
                        route->second.Remove(conn->id);

						if (route->second.Empty())
							m_RoutingMap.erase(route);
                    }
                });

				m_ListeningMap.erase(listening);
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_ConnectionMutex);

			auto found = m_Connections.find(conn->id);
            if (found != m_Connections.end() && found->second == conn)
				m_Connections.erase(found);

			conn->retired_next = m_Retired;
            m_Retired = conn;
        }

        {
            std::lock_guard<std::mutex> lock(conn->send_mutex);

			shutdown_socket(conn->socket);
            close_socket(conn->socket);
            conn->socket = invalid_socket;
        }

        if (dispatch)
			DispatchEvent(DISCONNECT, conn->id);
    }


	bool RoutePacket(CPacket *packet, mqme::channel_t source)
    {
        bool success = true;

		std::lock_guard<std::mutex> routing_lock(m_RoutingMutex);

		auto route = m_RoutingMap.find(packet->GetContext());
        if (route == m_RoutingMap.end())
			return true;

        std::lock_guard<std::mutex> connection_lock(m_ConnectionMutex);

		route->second.ForEach([&](mqme::channel_t id)
        {
            if (id == source)
				return;

			auto found = m_Connections.find(id);
            if (found == m_Connections.end())
				return;

			SConnection* target = found->second;

			std::lock_guard<std::mutex> send_lock(target->send_mutex);

			if ((target->socket == invalid_socket) || !protocol::SendPacket(target->socket, packet))
				success = false;
        });

		return success;
    }

    void DispatchEvent(EventType event, mqme::channel_t generator)
    {
        EVENT_HANDLER handler{};

		{
            std::lock_guard<std::mutex> lock(m_HandlerMutex);
            auto found = m_EventHandlers.find(event);

			if (found != m_EventHandlers.end())
				handler = found->second;
        }

		if (!handler)
			return;

        BeginTask();
        g_ThreadPool->RunTask([this, handler, event, generator](size_t)
        {
            handler(this, event, generator);
            EndTask();

			return pool::IThreadPool::TaskReturn::OK;
        });
    }

    void DeleteConnections()
    {
        while (m_Retired)
        {
            SConnection* next = m_Retired->retired_next;
            delete m_Retired;
            m_Retired = next;
        }
    }


    std::atomic<bool> m_Running{false};
    std::atomic<socket_t> m_ListenSocket{invalid_socket};
    std::thread m_AcceptThread;
    std::thread m_ReceiveThread;

    std::mutex m_ConnectionMutex;
    std::condition_variable m_ConnectionCondition;
	using TChanConnMap = std::map<channel_t, SConnection *>;
    TChanConnMap m_Connections;
    SConnection *m_Retired = nullptr;
    std::vector<SConnection *> m_PollConnections;
    std::vector<pollfd_t> m_PollDescriptors;

    std::mutex m_RoutingMutex;
    std::map<channel_t, ChannelSet> m_RoutingMap;
    std::map<channel_t, ChannelSet> m_ListeningMap;

    std::mutex m_HandlerMutex;
    std::map<FOURCHARCODE, PACKET_HANDLER> m_PacketHandlers;
    std::map<EventType, EVENT_HANDLER> m_EventHandlers;

    std::mutex m_TaskMutex;
    std::condition_variable m_TaskCondition;
    size_t m_ActiveTasks = 0;
};


mqme::IServer* mqme::IServer::NewServer()
{
    return new CServer();
}
