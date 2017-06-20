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

#include "StdAfx.h"

#include <mqme.h>
#include <winsock2.h>
#include <ObjBase.h>

#include "Packet.h"
#include "PacketQueue.h"
#include "ThreadPool.h"

extern CThreadPool *g_ThreadPool;
extern bool g_Initialized;

class CCoreClient: public mqme::ICoreClient
{
protected:
	HANDLE m_RecvThread;
	DWORD m_RecvThreadId;

	HANDLE m_SendThread;
	DWORD m_SendThreadId;

	SOCKET m_Socket;
	HANDLE m_WSEvent;
	HANDLE m_QuitEvent;

	tstring m_ServerAddr;
	UINT16 m_ServerPort;

	CPacketQueue m_InPackets;
	CPacketQueue m_OutPackets;

	typedef struct sPacketHandlerCallInfo
	{
		sPacketHandlerCallInfo(PACKET_HANDLER _func, LPVOID _userdata) { func = _func; userdata = _userdata; }

		PACKET_HANDLER func;
		LPVOID userdata;
	} SPacketHandlerCallInfo;

	typedef std::map< FOURCHARCODE, SPacketHandlerCallInfo > TPacketHandlerMap;
	typedef std::pair< FOURCHARCODE, SPacketHandlerCallInfo > TPacketHandlerPair;

	TPacketHandlerMap m_PacketHandlerMap;

	typedef struct sEventHandlerCallInfo
	{
		sEventHandlerCallInfo(EVENT_HANDLER _func, LPVOID _userdata) { func = _func; userdata = _userdata; }

		EVENT_HANDLER func;
		LPVOID userdata;
	} SEventHandlerCallInfo;

	typedef std::map< EEventType, SEventHandlerCallInfo > TEventHandlerMap;
	typedef std::pair< EEventType, SEventHandlerCallInfo > TEventHandlerPair;

	TEventHandlerMap m_EventHandlerMap;

	GUID m_GUID;

public:
	CCoreClient()
	{
		m_Socket = INVALID_SOCKET;
		m_ServerAddr = _T("127.0.0.1");
		m_ServerPort = 8080;

		m_WSEvent = WSACreateEvent();
		m_QuitEvent = WSACreateEvent();

		m_RecvThread = NULL;
		m_RecvThreadId = 0;

		m_SendThread = NULL;
		m_SendThreadId = 0;

		CoCreateGuid(&m_GUID);
	}


	~CCoreClient()
	{
		Disconnect();
		CloseHandle(m_WSEvent);
		CloseHandle(m_QuitEvent);
	}

	// Releases the client, implicitly calling Disconnect
	// WARNING: Once the client has been released, do not
	// attempt to call any member functions.
	virtual void Release()
	{
		delete this;
	}

	void FlushOutgoingPackets()
	{
		while (!m_OutPackets.Empty())
		{
			CPacket *ppkt = m_OutPackets.Deque();
			if (ppkt)
				ppkt->Release();
		}
	}

	// Attempts to connect to the given server address
	// If myid is null, a new GUID will be generated and sent to the server
	virtual bool Connect(const TCHAR *address, UINT16 port, GUID *my_id = nullptr)
	{
		Disconnect();

		m_ServerAddr = address;
		m_ServerPort = port;
		if (my_id)
			m_GUID = *my_id;

		if (m_Socket == INVALID_SOCKET)
		{
			m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			if (m_Socket != INVALID_SOCKET)
			{
				sockaddr_in clientService;
				clientService.sin_family = AF_INET;

				char *tmpaddr;
				LOCAL_TCS2MBCS(m_ServerAddr.c_str(), tmpaddr);

				if (isalpha(tmpaddr[0]))
				{
					struct hostent *remotehost;
					remotehost = gethostbyname(tmpaddr);
					clientService.sin_addr.s_addr = remotehost ? *((UINT32 *)remotehost->h_addr_list[0]) : inet_addr("127.0.0.1");
				}
				else
				{
					clientService.sin_addr.s_addr = inet_addr(tmpaddr);
				}

				clientService.sin_port = htons(m_ServerPort);

				if (connect(m_Socket, (SOCKADDR *)&clientService, sizeof(clientService)) == SOCKET_ERROR)
				{
					closesocket(m_Socket);
					m_Socket = NULL;

					int err = WSAGetLastError();

					return false;
				}

				WSABUF buf;
				DWORD flags = 0;
				DWORD ct = 0;
				int q;

				// the first thing a client sends to a server upon connection is it's GUID
				buf.buf = (char *)&m_GUID;
				buf.len = sizeof(GUID);
				q = WSASend(m_Socket, &buf, 1, &ct, 0, NULL, NULL);

				WSAEventSelect(m_Socket, m_WSEvent, FD_CLOSE | FD_READ);

				if (!m_SendThread)
				{
					m_SendThread = CreateThread(NULL, 1 << 16, SendThreadProc, this, 0, &m_SendThreadId);
				}

				if (!m_RecvThread)
				{
					m_RecvThread = CreateThread(NULL, 1 << 16, RecvThreadProc, this, 0, &m_RecvThreadId);
				}
			}
		}

		return true;
	}

	// Returns the ID that was provided to, or generated by, the call to Connect
	// Useful to persist connection identity
	virtual GUID GetID()
	{
		return m_GUID;
	}

	// Disconnects from the server, if connected
	virtual void Disconnect()
	{
		if (m_Socket != INVALID_SOCKET)
		{
			closesocket(m_Socket);
			m_Socket = INVALID_SOCKET;

			if (m_SendThread)
			{
				SignalObjectAndWait(m_QuitEvent, m_SendThread, INFINITE, false);
				m_SendThread = NULL;
			}

			// if the thread is running, then 
			if (m_RecvThread)
			{
				SignalObjectAndWait(m_QuitEvent, m_RecvThread, INFINITE, false);
				m_RecvThread = NULL;
			}

			WSAResetEvent(m_QuitEvent);
		}

		FlushOutgoingPackets();
	}

	// Returns the state of connection
	virtual bool IsConnected()
	{
		return (m_Socket != INVALID_SOCKET);
	}

	// Sends a packet -- once a packet has been sent,
	// it should not be modified
	virtual bool SendPacket(ICorePacket *packet)
	{
		CPacket *p = dynamic_cast<CPacket *>(packet);
		if (p)
		{
			m_OutPackets.Enque(p);

			return true;
		}

		// You're hosed
		return false;
	}

	// Registers an incoming packet handling callback with the client.
	// When a packet with the given id arrives, this callback
	// will be executed.
	virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler, LPVOID userdata = nullptr)
	{
		TPacketHandlerMap::const_iterator cit = m_PacketHandlerMap.find(id);
		if (cit == m_PacketHandlerMap.end())
		{
			m_PacketHandlerMap.insert(TPacketHandlerPair(id, SPacketHandlerCallInfo(handler, userdata)));
		}
	}

	// Registers an event handling callback with the client.
	virtual void RegisterEventHandler(EEventType ev, EVENT_HANDLER handler, LPVOID userdata = nullptr)
	{
		TEventHandlerMap::const_iterator cit = m_EventHandlerMap.find(ev);
		if (cit == m_EventHandlerMap.end())
		{
			m_EventHandlerMap.insert(TEventHandlerPair(ev, SEventHandlerCallInfo(handler, userdata)));
		}
	}

private:
	static void WINAPI ProcessPacket(LPVOID param0, LPVOID param1, LPVOID param2)
	{
		CCoreClient *_this = (CCoreClient *)param0;
		CPacket *ppkt = (CPacket *)param1;

		// look up the packet handler and call it with the appropriate parameters
		TPacketHandlerMap::iterator it = _this->m_PacketHandlerMap.find(ppkt->GetID());
		if (it != _this->m_PacketHandlerMap.end())
		{
			it->second.func(_this, ppkt, it->second.userdata);
		}

		ppkt->Release();
	}

	static void WINAPI PrivateDisconnect(LPVOID param0, LPVOID param1, LPVOID param2)
	{
		CCoreClient *_this = (CCoreClient *)param0;

		_this->Disconnect();
	}

	static DWORD WINAPI RecvThreadProc(LPVOID param)
	{
		CCoreClient *_this = (CCoreClient *)param;
		if (_this)
		{
			HANDLE evs[2];
			evs[0] = _this->m_QuitEvent;
			evs[1] = _this->m_WSEvent;

			while (true)
			{
				if (WSAWaitForMultipleEvents(2, evs, false, INFINITE, false) == WSA_WAIT_EVENT_0)
				{
					break;
				}

				WSANETWORKEVENTS ne;
				if (WSAEnumNetworkEvents(_this->m_Socket, _this->m_WSEvent, &ne) == 0)
				{
					if (ne.lNetworkEvents & FD_CLOSE)
					{
						g_ThreadPool->RunTask(PrivateDisconnect, (LPVOID)_this);
						break;
					}
				}

				CPacket *ppkt = (CPacket *)mqme::ICorePacket::NewPacket();

				SPacketHeader pkthdr;
				memset(&pkthdr, 0, sizeof(SPacketHeader));

				WSABUF buf;
				buf.buf = (char *)&pkthdr;
				buf.len = sizeof(SPacketHeader);

				DWORD flags = 0;
				DWORD rct = 0;

				int q = WSARecv(_this->m_Socket, &buf, 1, &rct, &flags, NULL, NULL);

				if (q != SOCKET_ERROR)
				{
					// allocate space in the packet
					ppkt->SetData(pkthdr.m_ID, pkthdr.m_DataLength, NULL);
					ppkt->SetContext(pkthdr.m_Context);
					ppkt->SetSender(pkthdr.m_Sender);

					buf.buf = (char *)ppkt->GetData();
					buf.len = ppkt->GetDataLength();
					if (buf.len > 0)
					{
						q = WSARecv(_this->m_Socket, &buf, 1, &rct, &flags, NULL, NULL);
					}
				}

				if (q != SOCKET_ERROR)
				{
					g_ThreadPool->RunTask(ProcessPacket, (LPVOID)_this, (LPVOID)ppkt);
				}
			}
		}

		return 0;
	}


	static DWORD WINAPI SendThreadProc(LPVOID param)
	{
		CCoreClient *_this = (CCoreClient *)param;
		if (_this)
		{
			while (true)
			{
				if (WSAWaitForMultipleEvents(1, &_this->m_QuitEvent, true, 75, false) == WSA_WAIT_EVENT_0)
				{
					break;
				}

				while (!_this->m_OutPackets.Empty())
				{
					CPacket *ppkt = _this->m_OutPackets.Deque();
					if (ppkt)
					{
						ppkt->SetSender(_this->m_GUID);

						WSABUF buf[2];
						buf[0].buf = (char *)ppkt->GetHeader();
						buf[0].len = ppkt->GetHeaderLength();
						buf[1].buf = (char *)ppkt->GetData();
						buf[1].len = ppkt->GetDataLength();

						DWORD sct = 0;

						int q = WSASend(_this->m_Socket, buf, 2, &sct, 0, NULL, NULL);

						ppkt->Release();

						if (q == SOCKET_ERROR)
						{
							DWORD err = WSAGetLastError();
						}
					}

					Sleep(0);
				}
			}
		}

		return 0;
	}

};

mqme::ICoreClient *mqme::ICoreClient::NewClient()
{
	if (!g_Initialized)
	{
		return NULL;
	}
	return (ICoreClient *)(new CCoreClient());
}
