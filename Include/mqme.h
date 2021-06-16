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

#include <stdint.h>
#include <guiddef.h>


#ifdef MQME_EXPORTS
#define MQME_API __declspec(dllexport)
#else
#define MQME_API __declspec(dllimport)
#endif


namespace mqme
{

typedef uint32_t FOURCHARCODE;


/// Initializes the mqme library
/// initial_idle_packet_count dictates how many packets of initial_packet_size will
/// be created and waiting to be used. If a larger size is needed, that packet is resized.
/// If more packets are required, more will be created at that time.
/// mqme uses a pool of worker threads to process incoming packets - the number of threads
/// available to mqme is controllable by setting the number of threads per core and an
/// additive modifier to that number of cores 
MQME_API bool Initialize(UINT initial_idle_packet_count = 256, UINT initial_packet_size = 4096, UINT threads_per_core = 1, INT core_count_adjustment = -2);


/// Closes the mqme library
MQME_API void Close();


/// ICorePacket interface -- allows user code to safely access arriving data
/// or to fill out data to be sent.  Notification callbacks will provide a
/// pointer to this interface type.
class ICorePacket
{
public:

	/// Releases the packet when you are done using it
	virtual void Release() = NULL;

	/// Sets the recipient of the packet (for routing)
	virtual void SetContext(GUID context) = NULL;

	/// Gets the context GUID
	virtual GUID GetContext() = NULL;

	/// Gets the GUID of the sender
	/// Note: set under the covers when a packet is sent
	virtual GUID GetSender() = NULL;

	/// Sets the id, data length, and the actual data in the packet
	/// Note: this copies the memory provided into private storage
	/// Also note: zero-length packets are just fine
	virtual void SetData(FOURCHARCODE id, uint32_t datalen, const BYTE *data) = NULL;

	/// Returns the packet identifier
	virtual FOURCHARCODE GetID() = NULL;

	/// Returns the length of the data stored in this packet
	virtual uint32_t GetDataLength() = NULL;

	/// Returns a pointer to the physical data stored in this packet
	virtual BYTE *GetData() = NULL;

	/// Returns an ICorePacket interface
	/// It should be noted that packets are globally managed
	/// and will be recycled when Released, so
	/// it is important to fill them out entirely
	/// before using them!
	MQME_API static ICorePacket *NewPacket();
};


/// IGUIDSet is a set of GUIDs that is used for routing packets appropriately. An instance of
/// ICoreServer will maintain listener data per-channel and it can be modified with this interface.
class IGUIDSet
{
public:
	/// Adds the given GUID to the set
	virtual void Add(GUID id) = NULL;

	/// Removes the given GUID from the set
	virtual void Remove(GUID id) = NULL;

	/// Returns true if the set contains the given GUID, false if it does not.
	virtual bool Contains(GUID id) = NULL;

	/// Returns the number of elements in the set
	virtual size_t Size() = NULL;

	/// Returns true if the set if empty, false if there is anything in it
	virtual bool Empty() = NULL;

	/// A parameter to the ForEach function; will be called for each GUID in the set
	typedef void(__cdecl *EACH_GUID_FUNC)(GUID id, void *userdata1, void *userdata2);

	/// Calls the given user-defined function back for each GUID in the set. Passes it userdata1 and userdata2
	virtual void ForEach(EACH_GUID_FUNC func, void *userdata1 = nullptr, void *userdata2 = nullptr) = NULL;
};


/// ICoreServer interface -- listens for incoming ICoreClient connections over TCP and
/// then provides routing capability for packets, forwarding them to other clients
/// in the same channel. AddListenerToChannel is called to add a given client to a channel,
/// but it is up to the server implementation to determine how and when to do that (the
/// TestServer / TestClient sample applications make a 'JOIN' request). Additionally,
/// the server can process packets of certain types and do whatever they want with the data
/// therein.
class ICoreServer
{
public:

	enum EEventType
	{
		ET_NONE = 0,

		ET_CONNECT,			/// a new client has connected
		ET_DISCONNECT,		/// a client has disconnected

		ET_NUMEVENTS
	};

	/// PACKET_HANDLER is a callback function provided by the user
	/// that will be called when a packet matching the type given in the
	/// ICoreServer::RegisterHandler arrives.  This callback will be given
	/// the ICoreServer interface which received the data, the packet itself, and
	/// the client from which the packet arrived
	typedef bool (__cdecl *PACKET_HANDLER)(ICoreServer *server, ICorePacket *packet, LPVOID userdata);

	/// The EVENT_HANDLER is a callback function provided by the user
	/// that will be called when an event specified in the ICoreServer::EEventType
	/// occurs.
	typedef bool (__cdecl *EVENT_HANDLER)(ICoreServer *server, EEventType ev, GUID generator, LPVOID userdata);

	/// Releases the server, implicitly calling StopListening
	virtual void Release() = NULL;

	/// Starts listening (and everything that entails) on the given
	/// port number, waiting for incoming connections and
	/// receiving and routing packets.
	virtual bool StartListening(uint16_t port) = NULL;

	/// Stops the server from operating, freeing all memory
	/// allocated by the server, stopping all threads, etc.
	virtual bool StopListening() = NULL;

	/// Sends a packet.
	/// NOTE: once a packet has been sent, it should not be modified
	virtual bool SendPacket(ICorePacket *packet) = NULL;

	/// This will add a connection to the routing table for the given channel.
	/// After this is called, packets sent to the channel will be sent to the listener
	virtual bool AddListenerToChannel(GUID channel, GUID listener) = NULL;

	/// This will add a connection to the routing table for the given channel.
	/// After this is called, packets sent to the channel will be sent to the listener
	virtual void RemoveListenerFromChannel(GUID channel, GUID listener) = NULL;

	/// Populates a set with the current listeners on a given channel
	virtual bool GetListeners(GUID channel, IGUIDSet **listeners) = NULL;

	/// Registers an incoming packet handling callback with the server.
	/// When a packet with the given id arrives, this callback
	/// will be executed.
	/// NOTE: packets will still be routed to their given context, even if no
	/// handler has been registered with the server
	virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler, LPVOID userdata = nullptr) = NULL;

	/// Registers an event handling callback with the server.
	virtual void RegisterEventHandler(EEventType ev, EVENT_HANDLER handler, LPVOID userdata = nullptr) = NULL;

	/// Instantiates a new server object
	MQME_API static ICoreServer *NewServer();
};


/// ICoreClient interface -- Connects to ICoreServer instances, subsequently sending and
/// receiving packets therefrom.
class ICoreClient
{
public:

	enum EEventType
	{
		ET_NONE = 0,

		ET_CONNECTED,			/// this client has been connected
		ET_DISCONNECTED,		/// this client has been disconnected

		ET_NUMEVENTS
	};

	/// The PACKET_HANDLER is a callback function provided by the user
	/// that will be called when a packet matching the type given in the
	/// ICoreClient::RegisterPacketHandler arrives.  This callback will be given
	/// the ICoreClient interface which received the data and the packet itself.
	typedef bool (__cdecl *PACKET_HANDLER)(ICoreClient *client, ICorePacket *packet, LPVOID userdata);

	/// The EVENT_HANDLER is a callback function provided by the user
	/// that will be called when an event specified in the ICoreClient::EEventType
	/// occurs.
	typedef bool(__cdecl *EVENT_HANDLER)(ICoreClient *client, EEventType ev, LPVOID userdata);

	/// Releases the client, implicitly calling Disconnect
	/// WARNING: Once the client has been released, do not
	/// attempt to call any member functions.
	virtual void Release() = NULL;

	/// Attempts to connect to the given server address
	/// If myid is null, a new GUID will be generated and sent to the server - 
	/// this allows a client to connect with a previously used GUID
	virtual bool Connect(const TCHAR *address, uint16_t port, GUID *my_id = nullptr) = NULL;

	/// Returns the ID that was provided to, or generated by, the call to Connect
	/// Useful to persist connection identity
	virtual GUID GetID() = NULL;

	/// Disconnects from the server, if connected
	virtual void Disconnect() = NULL;

	/// Returns the state of connection
	virtual bool IsConnected() = NULL;

	/// Sends a packet.
	/// NOTE: once a packet has been sent, it should not be modified
	virtual bool SendPacket(ICorePacket *packet) = NULL;

	/// Registers an incoming packet handling callback with the client.
	/// When a packet with the given id arrives, this callback
	/// will be executed.
	virtual void RegisterPacketHandler(FOURCHARCODE id, PACKET_HANDLER handler, LPVOID userdata = nullptr) = NULL;

	/// Registers an event handling callback with the client.
	virtual void RegisterEventHandler(EEventType ev, EVENT_HANDLER handler, LPVOID userdata = nullptr) = NULL;

	/// Instantiates a new client object
	MQME_API static ICoreClient *NewClient();
};


};
