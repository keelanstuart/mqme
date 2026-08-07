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

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

#if defined(_WIN32)
#include <guiddef.h>
#endif


#if defined(_WIN32) && !defined(MQME_STATIC)

#if defined(MQME_EXPORTS)
#define MQME_API __declspec(dllexport)
#else
#define MQME_API __declspec(dllimport)
#endif

#elif defined(__GNUC__) && !defined(MQME_STATIC)

#define MQME_API __attribute__((visibility("default")))

#else

#define MQME_API

#endif


namespace mqme
{

	using FOURCHARCODE = std::uint32_t;


	struct channel_t
	{
#pragma pack(push, 1)

		union
		{
			uint8_t m_GuidBytes[16];

#if defined(_WIN32)
			GUID m_Guid;
#endif
		};

#pragma pack(pop)

	    bool operator ==(const channel_t &other) const
    	{
        	return std::memcmp(m_GuidBytes, other.m_GuidBytes, sizeof(m_GuidBytes)) == 0;
	    }

	    bool operator !=(const channel_t &other) const
		{
			return !(*this == other);
		}

	    bool operator <(const channel_t &other) const
	    {
    	    return std::memcmp(m_GuidBytes, other.m_GuidBytes, sizeof(m_GuidBytes)) < 0;
	    }

	};

	static_assert(sizeof(channel_t) == 16, "channel_t must be 16 bytes");


	// Initializes the mqme library
	// initial_idle_packet_count dictates how many packets of initial_packet_size will
	// be created and waiting to be used. If a larger size is needed, that packet is resized.
	// If more packets are required, more will be created at that time.
	// mqme uses a pool of worker threads to process incoming packets - the number of threads
	// available to mqme is controllable by setting the number of threads per core and an
	// additive modifier to that number of cores 
	MQME_API bool Initialize(
		size_t initial_idle_packet_count = 256,
		size_t initial_packet_size = 4096,
        size_t threads_per_core = 1,
		int core_count_adjustment = -2);

	// Closes the mqme library
	MQME_API void Close();

	// Generates a new channel identifier
	MQME_API channel_t GenerateChannel();

	// Returns whether or not the channel is "null" (generally refers to the server)
	MQME_API channel_t NullChannel();


	// IPacket interface -- allows user code to safely access arriving data
	// or to fill out data to be sent.  Notification callbacks will provide a
	// pointer to this interface type.
	class IPacket
	{

	public:

		// Releases the packet when you are done using it
	    virtual void Release() = 0;

		// Sets the id, data length, and the actual data in the packet
		// Note: this copies the memory provided into private storage
		// Also note: zero-length packets are just fine
		virtual void SetData(FOURCHARCODE id, size_t datalen, const void* data) = 0;

		// Sets the recipient of the packet (for routing)
    	virtual void SetContext(channel_t context) = 0;

		// Gets the channel
    	virtual channel_t GetContext() const = 0;

		// Gets the channel of the sender
		// Note: set under the covers when a packet is sent
	    virtual channel_t GetSender() const = 0;

		// Returns the packet identifier
    	virtual FOURCHARCODE GetID() const = 0;

		// Returns the length of the data stored in this packet
	    virtual size_t GetDataLength() const = 0;

		// Returns a pointer to the physical data stored in this packet
	    virtual const uint8_t *GetData() const = 0;

		// Returns an IPacket interface
		// It should be noted that packets are globally managed
		// and will be recycled when Released, so
		// it is important to fill them out entirely
		// before using them!
	    MQME_API static IPacket *NewPacket();

	};


	class IChannelSet
	{

	public:

		// Adds a channel to the set
		virtual void Add(channel_t id) = 0;

		// Removes a channel from the set
		virtual void Remove(channel_t id) = 0;

		// Returns true if the set contains the given channel, false if it does not.
		virtual bool Contains(channel_t id) const = 0;

		// Returns the number of channels in the set
		virtual size_t Size() const = 0;

		// Returns true if the set if empty, false if there is anything in it
		virtual bool Empty() const = 0;

		// A parameter to the ForEach function; will be called for each channel in the set
		using PerChannelFunc = std::function<void(channel_t)>;
 
		// Calls the given user-defined function back for each channel_t in the set. Passes it userdata1 and userdata2
    	virtual void ForEach(PerChannelFunc func) const = 0;

	};


	// IServer interface -- listens for incoming IClient connections over TCP and
	// then provides routing capability for packets, forwarding them to other clients
	// in the same channel. AddListenerToChannel is called to add a given client to a channel,
	// but it is up to the server implementation to determine how and when to do that (the
	// TestServer / TestClient sample applications make a 'JOIN' request). Additionally,
	// the server can process packets of certain types and do whatever they want with the data
	// therein.
	class IServer
	{

	public:

		using EventType = enum
		{
			NONE = 0,

			CONNECT,			// a new client has connected
			DISCONNECT,			// a client has disconnected

			NUMEVENTS
		};

		// PACKET_HANDLER is a callback function provided by the user
		// that will be called when a packet matching the type given in the
		// IServer::RegisterHandler arrives.  This callback will be given
		// the IServer interface which received the data, the packet itself, and
		// the client from which the packet arrived
	    using PACKET_HANDLER = std::function<bool(IServer *, IPacket *)>;

		// The EVENT_HANDLER is a callback function provided by the user
		// that will be called when an event specified in the IServer::EEventType
		// occurs.
    	using EVENT_HANDLER = std::function<bool(IServer *, EventType, channel_t)>;

		// Releases the server, implicitly calling StopListening
	    virtual void Release() = 0;

		// Starts listening (and everything that entails) on the given
		// port number, waiting for incoming connections and
		// receiving and routing packets.
    	virtual bool StartListening(std::uint16_t port) = 0;

		// Stops the server from operating, freeing all memory
		// allocated by the server, stopping all threads, etc.
	    virtual bool StopListening() = 0;

		// Sends a packet.
		// NOTE: once a packet has been sent, it should not be modified
	    virtual bool SendPacket(IPacket *packet) = 0;

		// This will add a connection to the routing table for the given channel.
		// After this is called, packets sent to the channel will be sent to the listener
	    virtual bool AddListenerToChannel(channel_t channel, channel_t listener) = 0;

		// This will add a connection to the routing table for the given channel.
		// After this is called, packets sent to the channel will be sent to the listener
	    virtual void RemoveListenerFromChannel(channel_t channel, channel_t listener) = 0;

		// Populates a set with the current listeners on a given channel
    	virtual bool GetListeners(channel_t channel, IChannelSet **listeners) = 0;
		// Registers an incoming packet handling callback with the server.
		// When a packet with the given id arrives, this callback
		// will be executed.
		// NOTE: packets will still be routed to their given context, even if no
		// handler has been registered with the server
	    virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler) = 0;
		// Registers an event handling callback with the server.
	    virtual void RegisterEventHandler(EventType ev, EVENT_HANDLER handler) = 0;

		// Instantiates a new server object
    	MQME_API static IServer *NewServer();

	};


	// IClient interface -- Connects to IServer instances, subsequently sending and
	// receiving packets therefrom.
	class IClient
	{
	
	public:

		using EventType = enum
		{
			NONE = 0,

			CONNECTED,			// this client has been connected
			DISCONNECTED,		// this client has been disconnected

			NUMEVENTS
		};

		// The PACKET_HANDLER is a callback function provided by the user
		// that will be called when a packet matching the type given in the
		// IClient::RegisterPacketHandler arrives.  This callback will be given
		// the IClient interface which received the data and the packet itself.
		using PACKET_HANDLER = std::function<bool(IClient *, IPacket *)>;

		// The EVENT_HANDLER is a callback function provided by the user
		// that will be called when an event specified in the IClient::EEventType
		// occurs.
    	using EVENT_HANDLER = std::function<bool(IClient *, EventType)>;

		// Releases the client, implicitly calling Disconnect
		// WARNING: Once the client has been released, do not
		// attempt to call any member functions.
	    virtual void Release() = 0;
		// Attempts to connect to the given server address
		// If myid is null, a new channel_t will be generated and sent to the server - 
		// this allows a client to connect with a previously used channel_t
	    virtual bool Connect(const char *address, uint16_t port, const channel_t *my_id = nullptr) = 0;

		// Returns the ID that was provided to, or generated by, the call to Connect
		// Useful to persist connection identity
	    virtual channel_t GetID() const = 0;

		// Disconnects from the server, if connected
    	virtual void Disconnect() = 0;

		// Returns the state of connection
    	virtual bool IsConnected() const = 0;

		// Sends a packet.
		// NOTE: once a packet has been sent, it should not be modified
	    virtual bool SendPacket(IPacket* packet) = 0;

		// Registers an incoming packet handling callback with the client.
		// When a packet with the given id arrives, this callback
		// will be executed.
    	virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler) = 0;

		// Registers an event handling callback with the client.
	    virtual void RegisterEventHandler(EventType ev, EVENT_HANDLER handler) = 0;

		// Instantiates a new client object
    	MQME_API static IClient* NewClient();

	};

};

