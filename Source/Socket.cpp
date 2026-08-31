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

#include "Socket.h"
#include <algorithm>
#include <climits>


bool socket_platform_initialize()
{

#if defined(_WIN32)

	WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;

#else

	return true;

#endif

}


void socket_platform_close()
{

#if defined(_WIN32)
    
	WSACleanup();

#endif

}


void close_socket(socket_t socket)
{
    if (socket == invalid_socket)
		return;

#if defined(_WIN32)

	closesocket(socket);

#else

	close(socket);

#endif

}


void shutdown_socket(socket_t socket)
{
    if (socket == invalid_socket)
		return;

#if defined(_WIN32)

	shutdown(socket, SD_BOTH);

#else

	shutdown(socket, SHUT_RDWR);

#endif

}


bool send_all(socket_t socket, const void* buffer, std::size_t size)
{
    const char* bytes = static_cast<const char*>(buffer);

	while (size)
    {

#if defined(_WIN32)

		const int amount = static_cast<int>(std::min<std::size_t>(size, INT_MAX));
        const int sent = ::send(socket, bytes, amount, 0);

#else

		const ssize_t sent = ::send(socket, bytes, size, MSG_NOSIGNAL);

#endif

		if (sent <= 0)
			return false;

		bytes += sent;
        size -= static_cast<std::size_t>(sent);
    }

	return true;
}


bool recv_all(socket_t socket, void* buffer, size_t size)
{
    char* bytes = (char *)buffer;

	while (size)
    {

#if defined(_WIN32)

		int amount = (int)std::min<size_t>(size, INT_MAX);
        int received = ::recv(socket, bytes, amount, 0);

#else

		const ssize_t received = ::recv(socket, bytes, size, 0);

#endif

		if (received <= 0)
			return false;

		bytes += received;
        size -= received;
    }

	return true;
}


int poll_descriptors(pollfd_t* descriptors, size_t count, int timeout_ms)
{

#if defined(_WIN32)

	return WSAPoll(descriptors, (ULONG)std::min<size_t>(count, ULONG_MAX), timeout_ms);

#else

	return ::poll(descriptors, count, timeout_ms);

#endif

}


size_t data_available(socket_t socket, bool& error)
{
    error = false;

#if defined(_WIN32)

	u_long available = 0;
    if (ioctlsocket(socket, FIONREAD, &available) != 0)
		error = true;

#else

	int available = 0;
    if (ioctl(socket, FIONREAD, &available) != 0)
		error = true;

#endif

	return available;
}
