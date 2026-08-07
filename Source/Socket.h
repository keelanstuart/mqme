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

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)

#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
using socket_length_t = int;
using pollfd_t = WSAPOLLFD;
constexpr socket_t invalid_socket = INVALID_SOCKET;

#else

#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
using socket_length_t = socklen_t;
using pollfd_t = pollfd;
constexpr socket_t invalid_socket = -1;

#endif

bool socket_platform_initialize();
void socket_platform_close();
void close_socket(socket_t socket);
void shutdown_socket(socket_t socket);
bool send_all(socket_t socket, const void* data, size_t size);
bool recv_all(socket_t socket, void* data, size_t size);
int poll_descriptors(pollfd_t* descriptors, size_t count, int timeout_ms);
size_t data_available(socket_t socket, bool& error);
