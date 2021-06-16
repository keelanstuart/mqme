# mqme
mqme - A network message queuing library written in C++ for Windows platforms (x86 / x64)


****


mqme simplifies client-server network communications; there are three basic interfaces to work with (and most of the time, only two in a single application): client, server, and packet.

### Packets:
* have a type, represented by a four-character-code
* have a context (channel)
* recycle themselves to reduce or eliminate runtime allocations


### Servers:
* accept client connections
* have channels
* route received packets to clients in the context channel
* optionally process packets themselves
* optionally send server-origin packets to clients
* manage channel members


### Clients:
* connect to a server
* optionally send packets to server
* optionally process packets


****


### Getting Started

First, call mqme::Initialize to start things up... you have four things to decide (but there are defaults!):
* how many packets should initially be in the packet cache
* the initial maximum size of each packets
* the number of threads per core to use for processing incoming packets
* a modifier to the number of cores
	
Next, create a client or server using either mqme::ICoreClient::NewClient() or mqme::ICoreServer::NewServer() ...

With the interface returned from one of those calls, you can register event- (think connection or disconnection) and received-packet-handling callbacks.

After that, call StartListening (mqme::ICoreServer) or Connect (mqme::ICoreClient) and your callbacks are active. When an event occurs or a packet is received, your code will be run.

When you want to send data, call mqme::ICorePacket::NewPacket() to get a packet from the cache. On the returned interface, call mqme::ICorePacket's SetContext and SetData members, then SendPacket (on either mqme::ICoreClient or mqme::ICoreServer). When sending a packet, there is no need to Release it -- the internals will do that automatically. When receiving a packet, you must call it's Release method after you are done examining it.

When you're all done, call Disconnect (mqme::ICoreClient) or StopListening (mqme::ICoreServer), followed by mqme::Close().


****


### Contexts: a Primer

A "context", in the mqme world, is a GUID.

When a client connects to a server, it can provide a GUID (for a persistent, existing identity) or one can be generated on the fly. Packets sent from one client directly to another client should be assigned the GUID of the recipient as the context.

To send a packet only to the server, the context GUID should be blank (zeroed).

It is up to the application as to how individual clients are added to, or removed from, a channel (or even how a client obtains information about available channels or other clients), but once multiple clients belong to the same channel (just another GUID!), any packets received with the channel's context are distributed to other clients in that channel (except the sender, if they are a member).
