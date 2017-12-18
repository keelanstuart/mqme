// TestServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <mqme.h>

CRITICAL_SECTION csprt;

bool HandlePacket(mqme::ICoreServer *server, mqme::ICorePacket *packet, LPVOID userdata)
{
	union
	{
		mqme::FOURCHARCODE id;
		char s[4];
	} ids;
	memset(&ids, 0, sizeof(mqme::FOURCHARCODE));

	ids.id = packet->GetID();
	GUID g = packet->GetSender();

	EnterCriticalSection(&csprt);
	_tprintf(_T("RX {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}: '%c%c%c%c'\n"),
		g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2],
		g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7],
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
			mqme::ICorePacket *pp = mqme::ICorePacket::NewPacket();
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
	}

	return true;
}

bool HandleEvent(mqme::ICoreServer *server, mqme::ICoreServer::EEventType ev, GUID g, LPVOID userdata)
{
	EnterCriticalSection(&csprt);

	switch (ev)
	{
		case mqme::ICoreServer::ET_CONNECT:
			_tprintf(_T("-- CONNECTION"));
			break;

		case mqme::ICoreServer::ET_DISCONNECT:
			_tprintf(_T("-- DISCONNECT"));
			break;
	}

	_tprintf(_T(" {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n"),
		g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2],
		g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);

	LeaveCriticalSection(&csprt);

	return true;
}

int main()
{
	InitializeCriticalSection(&csprt);

	if (mqme::Initialize())
	{
		mqme::ICoreServer *pServer = mqme::ICoreServer::NewServer();
		if (pServer)
		{
			pServer->RegisterPacketHandler('JOIN', HandlePacket, nullptr);
			pServer->RegisterPacketHandler('QUIT', HandlePacket, nullptr);
			pServer->RegisterPacketHandler('HELO', HandlePacket, nullptr);
			pServer->RegisterPacketHandler('PING', HandlePacket, nullptr);

			pServer->RegisterEventHandler(mqme::ICoreServer::ET_CONNECT, HandleEvent, nullptr);
			pServer->RegisterEventHandler(mqme::ICoreServer::ET_DISCONNECT, HandleEvent, nullptr);

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

