/*
	mqme Library Source File

	Copyright � 2009-2021, Keelan Stuart. All rights reserved.

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
#include <ObjBase.h>
#include <set>
#include <mutex>

#include "Packet.h"
#include "PacketQueue.h"
#include <Pool.h>

extern pool::IThreadPool *g_ThreadPool;
extern bool g_Initialized;


// In order to make TGUIDSet work, a Compare must be provided
struct GUIDComparer
{
	bool operator()(const GUID &a, const GUID &b) const
	{
		return (memcmp(&a, &b, sizeof(GUID)) < 0) ? true : false;
	}
};

typedef std::set< GUID, GUIDComparer > TGUIDSet;

class CGUIDSet : public IGUIDSet
{
public:
	CGUIDSet()
	{
	}

	virtual void Add(GUID id)
	{
		m_GUIDSet.insert(id);
	}

	virtual void Remove(GUID id)
	{
		m_GUIDSet.erase(m_GUIDSet.find(id));
	}

	virtual bool Contains(GUID id)
	{
		return (m_GUIDSet.find(id) == m_GUIDSet.end()) ? false : true;
	}

	virtual size_t Size()
	{
		return m_GUIDSet.size();
	}

	virtual bool Empty()
	{
		return m_GUIDSet.empty();
	}

	virtual void ForEach(EACH_GUID_FUNC func, void *userdata1, void *userdata2)
	{
		if (!func)
			return;

		for (const auto &it : m_GUIDSet)
		{
			func(it, userdata1, userdata2);
		}
	}

	TGUIDSet m_GUIDSet;
};

class CCoreServer: public mqme::ICoreServer
{
protected:
	HANDLE m_ListenThread;
	DWORD m_ListenThreadId;

	HANDLE m_RecvThread;
	DWORD m_RecvThreadId;

	HANDLE m_SendThread;
	DWORD m_SendThreadId;

	typedef struct sConnectionInfo
	{
		sConnectionInfo() { ZeroMemory(&addr, sizeof(sockaddr_in)); sock = NULL; ev = NULL; }

		sockaddr_in addr;
		SOCKET sock;
		HANDLE ev;
	} SConnectionInfo;

	typedef std::map<GUID, SConnectionInfo, GUIDComparer> TConnectionMap;
	TConnectionMap m_ConnectionMap;
	std::mutex m_ConnectionLock;


	HANDLE m_QuitEvent;
	uint16_t m_Port;

	typedef struct sPacketHandlerCallInfo
	{
		sPacketHandlerCallInfo(PACKET_HANDLER _func, void *_userdata) { func = _func; userdata = _userdata; }

		PACKET_HANDLER func;
		void *userdata;
	} SPacketHandlerCallInfo;

	typedef std::map< FOURCHARCODE, SPacketHandlerCallInfo > TPacketHandlerMap;

	TPacketHandlerMap m_PacketHandlerMap;

	typedef struct sEventHandlerCallInfo
	{
		sEventHandlerCallInfo(EVENT_HANDLER _func, void *_userdata) { func = _func; userdata = _userdata; }

		EVENT_HANDLER func;
		void *userdata;
	} SEventHandlerCallInfo;

	typedef std::map < EEventType, SEventHandlerCallInfo > TEventHandlerMap;

	TEventHandlerMap m_EventHandlerMap;

	typedef std::map< GUID, CGUIDSet, GUIDComparer > TGUIDSetMap;

	TGUIDSetMap m_RoutingTable;
	std::mutex m_RoutingLock;

	TGUIDSetMap m_ListeningTable;
	std::mutex m_ListeningLock;

	CPacketQueue m_Outgoing;

public:
	CCoreServer()
	{
		// 8080 is the default port we're on
		m_Port = 8080;

		m_ListenThread = NULL;
		m_ListenThreadId = 0;

		m_SendThread = NULL;
		m_SendThreadId = 0;

		m_RecvThread = NULL;
		m_RecvThreadId = 0;

		m_QuitEvent = WSACreateEvent();
	}

	~CCoreServer()
	{
		StopListening();
	}

	virtual void Release()
	{
		delete this;
	}

	void FlushOutgoingPackets()
	{
		while (!m_Outgoing.Empty())
		{
			CPacket *ppkt = m_Outgoing.Deque();
			if (ppkt)
				ppkt->Release();
		}
	}

	virtual bool StartListening(uint16_t port)
	{
		FlushOutgoingPackets();

		if (m_ListenThread && (port != m_Port))
		{
			StopListening();
		}

		m_Port = port;

		if (!m_ListenThread)
		{
			m_ListenThread = CreateThread(NULL, 1 << 17, ListenThreadProc, this, 0, &m_ListenThreadId);
		}

		if (!m_RecvThread)
		{
			m_RecvThread = CreateThread(NULL, 1 << 17, RecvThreadProc, this, 0, &m_RecvThreadId);
		}

		if (!m_SendThread)
		{
			m_SendThread = CreateThread(NULL, 1 << 17, SendThreadProc, this, 0, &m_SendThreadId);
		}

		return (m_ListenThreadId && m_RecvThreadId && m_SendThreadId) ? true : false;
	}

	virtual bool StopListening()
	{
		FlushOutgoingPackets();

		SignalObjectAndWait(m_QuitEvent, m_ListenThread, INFINITE, false);
		SignalObjectAndWait(m_QuitEvent, m_SendThread, INFINITE, false);
		SignalObjectAndWait(m_QuitEvent, m_RecvThread, INFINITE, false);

		return true;
	}

	virtual bool SendPacket(ICorePacket *packet)
	{
		if (packet)
		{
			((CPacket *)packet)->IncRef();
			m_Outgoing.Enque((CPacket *)packet);

			return true;
		}

		return false;
	}

	virtual bool AddListenerToChannel(GUID channel, GUID listener)
	{
		bool ret = true;

		TConnectionMap::const_iterator cit = m_ConnectionMap.find(listener);
		if (cit == m_ConnectionMap.end())
			ret = false;

		if (ret)
		{
			m_ListeningLock.lock();
			TGUIDSetMap::iterator lit = m_ListeningTable.find(listener);
			if (lit == m_ListeningTable.end())
			{
				std::pair<TGUIDSetMap::iterator, bool> insres = m_ListeningTable.insert(TGUIDSetMap::value_type(listener, CGUIDSet()));
				if (insres.second)
					lit = insres.first;
			}

			if (lit != m_ListeningTable.end())
				lit->second.Add(channel);
			m_ListeningLock.unlock();

			m_RoutingLock.lock();
			TGUIDSetMap::iterator rit = m_RoutingTable.find(channel);
			if (rit == m_RoutingTable.end())
			{
				std::pair<TGUIDSetMap::iterator, bool> insres = m_RoutingTable.insert(TGUIDSetMap::value_type(channel, CGUIDSet()));
				if (insres.second)
					rit = insres.first;
			}

			if (rit != m_RoutingTable.end())
				rit->second.Add(listener);
			m_RoutingLock.unlock();
		}

		return ret;
	}

	virtual void RemoveListenerFromChannel(GUID channel, GUID listener)
	{
		m_ListeningLock.lock();
		TGUIDSetMap::iterator lit = m_ListeningTable.find(listener);
		if (lit != m_ListeningTable.end())
		{
			lit->second.Remove(channel);
			if (lit->second.Empty())
				m_ListeningTable.erase(lit);
		}
		m_ListeningLock.unlock();

		m_RoutingLock.lock();
		TGUIDSetMap::iterator rit = m_RoutingTable.find(channel);
		if (rit != m_RoutingTable.end())
		{
			rit->second.Remove(listener);
			if (rit->second.Empty())
				m_RoutingTable.erase(rit);
		}
		m_RoutingLock.unlock();
	}

	virtual bool GetListeners(GUID channel, IGUIDSet **listeners)
	{
		bool ret = false;

		TGUIDSetMap::iterator rit = m_RoutingTable.find(channel);
		if (rit != m_RoutingTable.end())
		{
			*listeners = &(rit->second);
			ret = true;
		}

		return false;
	}

	virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler, void *userdata = nullptr)
	{
		TPacketHandlerMap::const_iterator cit = m_PacketHandlerMap.find(id);
		if (cit == m_PacketHandlerMap.end())
		{
			m_PacketHandlerMap.insert(TPacketHandlerMap::value_type(id, SPacketHandlerCallInfo(handler, userdata)));
		}
	}

	virtual void RegisterEventHandler(EEventType ev, EVENT_HANDLER handler, void *userdata = nullptr)
	{
		TEventHandlerMap::const_iterator cit = m_EventHandlerMap.find(ev);
		if (cit == m_EventHandlerMap.end())
		{
			m_EventHandlerMap.insert(TEventHandlerMap::value_type(ev, SEventHandlerCallInfo(handler, userdata)));
		}
	}

private:
	static DWORD WINAPI ListenThreadProc(void *param)
	{
		CCoreServer *_this = (CCoreServer *)param;

		SOCKET s = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (s == INVALID_SOCKET)
		{
			return -1;
		}

		SOCKADDR_IN sa;

		// Listen on our designated Port#
		sa.sin_port = htons(_this->m_Port);

		// Fill in the rest of the address structure
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = INADDR_ANY;

		// bind our name to the socket
		if (bind(s, (LPSOCKADDR)&sa, sizeof(struct sockaddr)) == SOCKET_ERROR)
		{
			return -1;
		}

		HANDLE events[2];
		events[0] = _this->m_QuitEvent;
		events[1] = WSACreateEvent();

		// enable accept and close events for our socket
		WSAEventSelect(s, events[1], FD_ACCEPT | FD_CLOSE);

		// listen for connections
		if (listen(s, 8) != SOCKET_ERROR)
		{
			while (true)
			{
				// Relinquish control until we get a new connection or we're supposed to quit
				DWORD waitret = WSAWaitForMultipleEvents(2, events, false, INFINITE, true);
				if (waitret == WSA_WAIT_EVENT_0)
				{
					break;
				}

				WSANETWORKEVENTS ne;
				if (WSAEnumNetworkEvents(s, events[1], &ne) == 0)
				{
					// Was a new connection established?
					if (ne.lNetworkEvents & FD_ACCEPT)
					{
						struct sockaddr_in clientaddr;
						memset(&clientaddr, 0, sizeof(struct sockaddr_in));

						// Accept it
						int addrlen = sizeof(struct sockaddr_in);
						SOCKET client_socket = WSAAccept(s, (sockaddr *)&clientaddr, &addrlen, NULL, NULL);

						if (client_socket != INVALID_SOCKET)
						{
							WSABUF buf;
							DWORD flags = 0;
							DWORD ct = 0;
							int q;

							GUID client_guid;

							// the client sends its GUID when it first connects
							buf.buf = (char *)&client_guid;
							buf.len = sizeof(GUID);
							q = WSARecv(client_socket, &buf, 1, &ct, &flags, NULL, NULL);

							SConnectionInfo cinf;
							cinf.addr = clientaddr;
							cinf.sock = client_socket;
							cinf.ev = WSACreateEvent();

							// have the new connection notify us if there's data available or it is closed/lost
							WSAEventSelect(client_socket, cinf.ev, FD_CLOSE | FD_READ);

							// set up our connectioon mapping
							_this->m_ConnectionMap.insert(TConnectionMap::value_type(client_guid, cinf));

							_this->m_RoutingLock.lock();
							std::pair<TGUIDSetMap::iterator, bool> rins = _this->m_RoutingTable.insert(TGUIDSetMap::value_type(client_guid, CGUIDSet()));
							rins.first->second.Add(client_guid);
							_this->m_RoutingLock.unlock();

							_this->m_ListeningLock.lock();
							std::pair<TGUIDSetMap::iterator, bool> lins = _this->m_ListeningTable.insert(TGUIDSetMap::value_type(client_guid, CGUIDSet()));
							lins.first->second.Add(client_guid);
							_this->m_ListeningLock.unlock();

							// if we have a registered packet handler, then schedule it to run
							TEventHandlerMap::iterator peit = _this->m_EventHandlerMap.find(ICoreServer::ET_CONNECT);
							if (peit != _this->m_EventHandlerMap.end())
								peit->second.func(_this, ICoreServer::ET_CONNECT, client_guid, peit->second.userdata);
						}
					}
					else if (ne.lNetworkEvents & FD_CLOSE)
					{
						break;
					}
				}

				WSAResetEvent(events[1]);
			}
		}

		CloseHandle(events[1]);

		closesocket(s);

		_this->m_ListenThread = NULL;
		_this->m_ListenThreadId = 0;

		WSAResetEvent(_this->m_QuitEvent);

		return 0;
	}


	static  pool::IThreadPool::TASK_RETURN __cdecl ProcessPacket(void *param0, void *param1, size_t task_number)
	{
		CCoreServer *_this = (CCoreServer *)param0;
		CPacket *ppkt = (CPacket *)param1;

		// look up the packet handler and call it with the appropriate parameters
		TPacketHandlerMap::iterator it = _this->m_PacketHandlerMap.find(ppkt->GetID());
		if (it != _this->m_PacketHandlerMap.end())
		{
			it->second.func(_this, ppkt, it->second.userdata);
		}

		ppkt->Release();

		return pool::IThreadPool::TR_OK;
	}

	static DWORD WINAPI RecvThreadProc(void *param)
	{
		CCoreServer *_this = (CCoreServer *)param;

		// traverse the connection map and see if data is available
		TConnectionMap::const_iterator it = _this->m_ConnectionMap.begin();
		while (true)
		{
			// is it time to quit?
			DWORD waitret = WaitForSingleObject(_this->m_QuitEvent, 0);
			if (waitret == WAIT_OBJECT_0)
				break;

			// if no connections, move on
			if (_this->m_ConnectionMap.empty())
			{
				Sleep(0);
				continue;
			}

			// time to start over?
			if (it == _this->m_ConnectionMap.end())
				it = _this->m_ConnectionMap.begin();

			HANDLE event = it->second.ev;
			SOCKET sock = it->second.sock;

			// there's no data... go to the next connection
			waitret = WSAWaitForMultipleEvents(1, &event, false, 0, true);
			if (waitret == WSA_WAIT_TIMEOUT)
			{
				Sleep(0);
				++it;
				continue;
			}

			WSANETWORKEVENTS ne;
			if (WSAEnumNetworkEvents(sock, event, &ne) == 0)
			{
				// is there data to read?
				if (ne.lNetworkEvents & FD_READ)
				{
					SPacketHeader pkthdr;
					memset(&pkthdr, 0, sizeof(SPacketHeader));

					WSABUF buf;
					DWORD flags = 0;
					DWORD rct = 0;

					// receive the packet header
					buf.buf = (char *)&pkthdr;
					buf.len = sizeof(SPacketHeader);
					int recvres = WSARecv(it->second.sock, &buf, 1, &rct, &flags, NULL, NULL);

					if (recvres != SOCKET_ERROR)
					{
						CPacket *ppkt = (CPacket *)mqme::ICorePacket::NewPacket();

						// allocate space in the packet
						ppkt->SetData(pkthdr.m_ID, pkthdr.m_DataLength, NULL);
						ppkt->SetContext(pkthdr.m_Context);
						ppkt->SetSender(it->first);

						// receive the rest of the packet if size was > 0
						buf.buf = (char *)ppkt->GetData();
						buf.len = ppkt->GetDataLength();
						if (buf.len > 0)
						{
							recvres = WSARecv(it->second.sock, &buf, 1, &rct, &flags, NULL, NULL);
						}

						if (recvres != SOCKET_ERROR)
						{
							GUID serverguid = { 0 };

							if (pkthdr.m_Context != serverguid)
							{
								// if the context exists
								TGUIDSetMap::iterator rit = _this->m_RoutingTable.find(pkthdr.m_Context);
								if (rit != _this->m_RoutingTable.end())
								{
									size_t num_listeners = rit->second.Size();

									// want to make sure that there's at least one client to route to,
									// but also that we're not re-transmitting to a single client - the sender
									if ((num_listeners > 1) || (!rit->second.Contains(ppkt->GetSender())))
									{
										// increment the ref count if we re-transmit to other listeners
										ppkt->IncRef();

										_this->m_Outgoing.Enque(ppkt);
									}
								}
							}

							// if we have a registered packet handler, then schedule it to run
							TPacketHandlerMap::iterator phit = _this->m_PacketHandlerMap.find(ppkt->GetID());
							if (phit != _this->m_PacketHandlerMap.end())
								g_ThreadPool->RunTask(ProcessPacket, (void *)_this, (void *)ppkt);
						}
					}
					else switch (WSAGetLastError())
					{
						case WSAENOTCONN:
						case WSAESHUTDOWN:
						case WSAECONNABORTED:
						case WSAECONNRESET:
						{
							break;
						}
					}

					WSAResetEvent(event);
				}
				else if (ne.lNetworkEvents & FD_CLOSE)
				{
					_this->m_ListeningLock.lock();
					_this->m_RoutingLock.lock();

					// find all the channels that this connection is listening to and remove it
					// from the routing table
					TGUIDSetMap::iterator lit = _this->m_ListeningTable.find(it->first);
					if (lit != _this->m_ListeningTable.end())
					{
						for (auto &elit : lit->second.m_GUIDSet)
						{
							TGUIDSetMap::iterator rit = _this->m_RoutingTable.find(elit);
							rit->second.Remove(it->first);
						}

						_this->m_ListeningTable.erase(lit);
					}

					_this->m_RoutingLock.unlock();
					_this->m_ListeningLock.unlock();

					closesocket(it->second.sock);
					CloseHandle(it->second.ev);

					// if we have a registered packet handler, then schedule it to run
					auto &peit = _this->m_EventHandlerMap.find(ICoreServer::ET_DISCONNECT);
					if (peit != _this->m_EventHandlerMap.end())
						peit->second.func(_this, ICoreServer::ET_DISCONNECT, it->first, peit->second.userdata);

					it = _this->m_ConnectionMap.erase(it);

					continue;
				}
			}

			it++;

			Sleep(0);
		}

		return 0;
	}

	static DWORD WINAPI SendThreadProc(void *param)
	{
		CCoreServer *_this = (CCoreServer *)param;

		HANDLE events[1];
		events[0] = _this->m_QuitEvent;

		while (true)
		{
			DWORD waitret = WSAWaitForMultipleEvents(1, events, false, 0, true);
			if (waitret == WSA_WAIT_EVENT_0)
				break;

			CPacket *ppkt = _this->m_Outgoing.Deque();
			if (ppkt)
			{
				// search the routing table for the channel given in the packet
				TGUIDSetMap::iterator cit = _this->m_RoutingTable.find(ppkt->GetHeader()->m_Context);
				if (cit != _this->m_RoutingTable.end())
				{
					// there are listeners on the given channel

					WSABUF buf[2];
					buf[0].buf = (char *)ppkt->GetHeader();
					buf[0].len = ppkt->GetHeaderLength();
					buf[1].buf = (char *)ppkt->GetData();
					buf[1].len = ppkt->GetDataLength();

					// send the packet to each listener
					for (auto &git : cit->second.m_GUIDSet)
					{
						if (git == ppkt->GetSender())
							continue;

						// get the socket for the listener's GUID
						auto &sit = _this->m_ConnectionMap.find(git);
						if (sit != _this->m_ConnectionMap.end())
						{
							DWORD sct;
							int sendret = WSASend(sit->second.sock, buf, 2, &sct, 0, nullptr, nullptr);

							if (sendret == SOCKET_ERROR)
							{
								switch (WSAGetLastError())
								{
									case WSAENOTCONN:
									case WSAESHUTDOWN:
									case WSAECONNABORTED:
									case WSAECONNRESET:
										break;
								}
							}
						}
					}
				}

				ppkt->Release();
			}
			else
			{
				Sleep(0);
			}
		}

		return 0;
	}
};

mqme::ICoreServer *mqme::ICoreServer::NewServer()
{
	if (!g_Initialized)
	{
		return NULL;
	}
	return (ICoreServer *)(new CCoreServer());
}
