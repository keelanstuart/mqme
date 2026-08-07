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

#include "Packet.h"
#include "Socket.h"
#include <cstdint>

namespace protocol
{

	constexpr std::uint32_t magic = 'MQME';
	constexpr std::uint16_t major = 2;
	constexpr std::uint16_t minor = 0;
	constexpr std::uint32_t feature_framed_packets = 1ull << 0;
	constexpr std::uint32_t feature_channel_routing = 1ull << 1;
	constexpr std::uint32_t supported_features = feature_framed_packets | feature_channel_routing;
	constexpr std::uint32_t max_payload = 64u * 1024u;


#pragma pack(push, 1)

	struct ClientHello
	{
		uint32_t magic;
		uint16_t major;
		uint16_t minor;
		uint32_t header_size;
		uint32_t features;
		channel_t client_id;
	};

	struct ServerHello
	{
		uint32_t magic;
		uint16_t major;
		uint16_t minor;
		uint32_t header_size;
		uint32_t features;
		uint32_t result;
		channel_t server_id;
	};

#pragma pack(pop)

	using ReceiveResult = enum
	{
		Complete,
		Incomplete,
		Disconnected,
		Error
	};

	inline ClientHello MakeClientHello(channel_t id)
	{
		ClientHello hello{};

		hello.magic = htonl(magic);
		hello.major = htons(major);
		hello.minor = htons(minor);
		hello.header_size = htonl(sizeof(ClientHello));
		hello.features = htonl(supported_features);
		hello.client_id = id;

		return hello;
	}

	inline ServerHello MakeServerHello(uint32_t result, channel_t server_id, uint32_t features)
	{
		ServerHello hello{};

		hello.magic = htonl(magic);
		hello.major = htons(major);
		hello.minor = htons(minor);
		hello.header_size = htonl(sizeof(ServerHello));
		hello.features = htonl(features);
		hello.result = htonl(result);
		hello.server_id = server_id;

		return hello;
	}

	inline bool ValidateClientHello(const ClientHello& hello)
	{
		return
			(ntohl(hello.magic) == magic) &&
			(ntohs(hello.major) == major) &&
			(ntohl(hello.header_size) == sizeof(ClientHello));
	}

	inline bool ValidateServerHello(const ServerHello& hello)
	{
		return
			(ntohl(hello.magic) == magic) &&
			(ntohs(hello.major) == major) &&
			(ntohl(hello.header_size) == sizeof(ServerHello)) &&
			(ntohl(hello.result) == 0);
	}

	inline bool SendPacket(socket_t socket, const CPacket *packet)
	{
		SPacketHeader h, *ph = (SPacketHeader *)(packet->GetHeader());
		memcpy(&h, ph, sizeof(SPacketHeader));
		h.ToNetwork();

		bool ret = send_all(socket, &h, sizeof(SPacketHeader));
		if (ret)
			ret &= send_all(socket, packet->GetData(), packet->GetDataLength());

		return ret;
	}

	inline ReceiveResult ReceivePacket(socket_t socket, CPacket*& packet)
	{
		packet = nullptr;
		bool error = false;
		size_t available = data_available(socket, error);

		if (error)
			return ReceiveResult::Error;

		if (!available || (available < sizeof(SPacketHeader)))
			return ReceiveResult::Incomplete;

		SPacketHeader header{};

#if defined(_WIN32)

		const int peeked = ::recv(socket, (char *)&header, sizeof(header), MSG_PEEK);

		if (peeked == 0)
			return ReceiveResult::Disconnected;

		if (peeked == SOCKET_ERROR)
			return ReceiveResult::Error;

#else

		const ssize_t peeked = ::recv(socket, &header, sizeof(header), MSG_PEEK | MSG_DONTWAIT);
		if (peeked == 0)
			return ReceiveResult::Disconnected;

		if (peeked < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK) return ReceiveResult::Incomplete;
			return ReceiveResult::Error;
		}

#endif

		if (static_cast<std::size_t>(peeked) < sizeof(SPacketHeader))
			return ReceiveResult::Incomplete;

		uint32_t header_length = ntohl(header.m_HeaderLength);
		uint32_t payload_length = ntohl(header.m_DataLength);

		if ((payload_length > max_payload) || (header_length != sizeof(SPacketHeader)))
			return ReceiveResult::Error;

		uint32_t frame_length = header_length + payload_length;

		if (available < frame_length)
			return ReceiveResult::Incomplete;

		packet = (CPacket *)mqme::IPacket::NewPacket();
		if (!packet)
			return ReceiveResult::Error;

		// allocate if necessary, but don't fill anything in -- recv will do that!
		packet->SetData(0, payload_length, nullptr);

		SPacketHeader *h = (SPacketHeader *)(packet->GetHeader());

		if (!recv_all(socket, (void *)(packet->GetHeader()), frame_length))
		{
			packet->Release();
			packet = nullptr;

			return ReceiveResult::Disconnected;
		}

		h->ToHost();

		return ReceiveResult::Complete;
	}
}
