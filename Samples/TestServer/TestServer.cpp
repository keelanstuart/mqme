// TestServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <mqme.h>

CRITICAL_SECTION csprt;


int main()
{
	InitializeCriticalSection(&csprt);

	if (mqme::Initialize())
	{
		mqme::IServer *pServer = mqme::IServer::NewServer();
		if (pServer)
		{
			mqme::IServer::PACKET_HANDLER HandlePacket = [](mqme::IServer *server, mqme::IPacket *packet)
			{
				union
				{
					mqme::FOURCHARCODE id;
					char s[4];
				} ids;
				memset(&ids, 0, sizeof(mqme::FOURCHARCODE));

				ids.id = packet->GetID();
				mqme::channel_t g = packet->GetSender();

				EnterCriticalSection(&csprt);
				_tprintf(_T("RX {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}: '%c%c%c%c'\n"),
						 g.m_Guid.Data1, g.m_Guid.Data2, g.m_Guid.Data3, g.m_Guid.Data4[0], g.m_Guid.Data4[1], g.m_Guid.Data4[2],
						 g.m_Guid.Data4[3], g.m_Guid.Data4[4], g.m_Guid.Data4[5], g.m_Guid.Data4[6], g.m_Guid.Data4[7],
						 ids.s[3], ids.s[2], ids.s[1], ids.s[0]);
				LeaveCriticalSection(&csprt);

				switch (packet->GetID())
				{
					case 'JOIN':
						server->AddListenerToChannel(packet->GetContext(), packet->GetSender());
						break;

					case 'QUIT':
						server->RemoveListenerFromChannel(packet->GetContext(), packet->GetSender());
						break;

					case 'HELO':
					{
						mqme::IPacket *pp = mqme::IPacket::NewPacket();
						if (pp)
						{
							pp->SetContext(packet->GetSender());
							pp->SetData('HIYA', 0, nullptr);
							server->SendPacket(pp);
						}
						break;
					}

					// 'PING' messages are only printed out - there's no special handling code otherwise
					// 'TEXT' messages are not processed by the server, only passed on to others in the context channel
					case 'TEXT':
					{
						EnterCriticalSection(&csprt);

						_tprintf(_T("%.16s\n"), (TCHAR *)packet->GetData());

						LeaveCriticalSection(&csprt);
						break;
					}
				}

				return true;
			};

			pServer->RegisterPacketHandler('JOIN', HandlePacket);
			pServer->RegisterPacketHandler('QUIT', HandlePacket);
			pServer->RegisterPacketHandler('HELO', HandlePacket);
			pServer->RegisterPacketHandler('TEXT', HandlePacket);
			pServer->RegisterPacketHandler('PING', HandlePacket);

			mqme::IServer::EVENT_HANDLER HandleEvent = [](mqme::IServer *server, mqme::IServer::EventType ev, mqme::channel_t g)
			{
				EnterCriticalSection(&csprt);

				switch (ev)
				{
					case mqme::IServer::EventType::CONNECT:
						_tprintf(_T("-- CONNECTION"));
						break;

					case mqme::IServer::EventType::DISCONNECT:
						_tprintf(_T("-- DISCONNECT"));
						break;
				}

				_tprintf(_T(" {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n"),
						 g.m_Guid.Data1, g.m_Guid.Data2, g.m_Guid.Data3, g.m_Guid.Data4[0], g.m_Guid.Data4[1], g.m_Guid.Data4[2],
						 g.m_Guid.Data4[3], g.m_Guid.Data4[4], g.m_Guid.Data4[5], g.m_Guid.Data4[6], g.m_Guid.Data4[7]);

				LeaveCriticalSection(&csprt);

				return true;
			};

			pServer->RegisterEventHandler(mqme::IServer::EventType::CONNECT, HandleEvent);
			pServer->RegisterEventHandler(mqme::IServer::EventType::DISCONNECT, HandleEvent);

			if (pServer->StartListening(12345))
			{
				while (!getc(stdin)) { Sleep(10); }

				pServer->StopListening();
			}

			pServer->Release();
		}

		mqme::Close();
	}

	DeleteCriticalSection(&csprt);

	return 0;
}

