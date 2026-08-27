# mqme

**Lightweight, channel-based network messaging for C++**

mqme (pronounced **"make me"**) is a small C++ networking library for applications that need multiple systems to exchange messages without building a messaging system from scratch.

Connect clients to a server, organize them into arbitrary channels, and send typed packets. mqme handles the connections, routing, threading, and packet reuse underneath.

A client can send a packet:

* directly to the server
* directly to another client
* to every other client listening to a channel

The same simple packet interface is used for all three.

mqme is designed to stay out of the way: there is no external broker, no service to install, and no heavyweight messaging framework between your application and the network.

---

## Why mqme?

Sometimes you don't need RabbitMQ, ZeroMQ, an HTTP API, or a distributed event platform.

Sometimes you have ten computers that need to say:

> **CALIBRATE**

Or a group of clients that need to share state with one another.

Or a server that needs to send an update to everyone interested in a particular thing.

That's the problem mqme solves.

```text
                    ┌──────────┐
               ┌────│ Client A │
               │    └──────────┘
               │
┌────────┐     │    ┌──────────┐
│ Server │─────┼────│ Client B │
└────────┘     │    └──────────┘
               │
               │    ┌──────────┐
               └────│ Client C │
                    └──────────┘

        A, B and C listen to Channel X

        A sends one packet to Channel X
                       │
                       ▼
              ┌────────┴────────┐
              ▼                 ▼
           Client B          Client C
```

The application decides what channels mean. They might represent rooms, devices, jobs, datasets, simulations, game sessions, sensor groups, or anything else that makes sense to your software.

mqme just routes the packets.

---

## A deliberately small model

There are three primary interfaces:

**Client**: connects to a server, sends packets, and receives packets and connection events.

**Server**: accepts clients, manages channel membership, routes packets, and can process or originate packets itself.

**Packet**: a typed block of data with a destination context.

That's essentially it.

Packets have a four-character type code that you provide:

```cpp
packet->SetData('DATA', size, data);
```

and a context that determines where they go:

```cpp
packet->SetContext(channel);
```

The context can identify a channel, an individual client, or the server itself.

---

## Channels

Channels are the heart of mqme.

Every client has a unique 128-bit identifier. That identifier also acts as the client's private channel, so sending directly to a client uses exactly the same mechanism as sending to a group.

An application-defined channel is simply another 128-bit identifier with multiple listeners.

```cpp
server->AddListenerToChannel(channel, client);
```

Once several clients are listening:

```text
Channel X
    ├── Client A
    ├── Client B
    └── Client C
```

a packet sent by Client A with Channel X as its context is automatically routed to the other listeners.

There is no separate broadcast API, direct-message API, room object, or subscription object. They are all variations of the same routing model.

A null context addresses the server itself.

---

## Designed for predictable runtime behavior

mqme was written as systems code, not as a wrapper around a collection of heavyweight networking abstractions.

Packets are cached and recycled. `NewPacket()` obtains a reusable packet rather than allocating a new packet object for every message. Packets have reusable internal data storage as well, reducing or eliminating heap allocation during normal message traffic.

The server uses a single socket-polling thread to watch connected clients. When data becomes available, receive work is dispatched to the thread pool rather than dedicating a permanently blocked thread to every connection.

The goal is straightforward:

> **Do the necessary work when something happens; do very little when nothing happens.**

---

## Cross-platform

mqme supports:

* Windows
* Linux
* x86 and x64 builds
* static and shared libraries

The networking implementation uses the native socket facilities on each platform behind a common interface.

CMake can fetch and build mqme's Pool dependency automatically.

---

## Getting started

Initialize mqme:

```cpp
mqme::Initialize();
```

The defaults create the packet cache and worker pool for you. Their sizing can also be configured explicitly when needed.

Create a server:

```cpp
mqme::IServer* server =
    mqme::IServer::NewServer();
```

Register a packet handler:

```cpp
server->RegisterPacketHandler(
    'HELO',
    [](mqme::IServer* server,
       mqme::IPacket* packet)
    {
        mqme::IPacket* response =
            mqme::IPacket::NewPacket();

        if (response)
        {
            response->SetContext(packet->GetSender());
            response->SetData('HIYA', 0, nullptr);

            server->SendPacket(response);
        }
    });
```

Register connection events:

```cpp
server->RegisterEventHandler(
    mqme::IServer::ET_CONNECT,
    [](mqme::IServer* server,
       mqme::IServer::EventType event,
       mqme::channel_t client)
    {
        // A client connected.
    });
```

Then start listening:

```cpp
if (server->StartListening(12345))
{
    // Your application runs normally while mqme
    // handles network traffic and callbacks.
}
```

---

## Sending packets

Obtain a packet from the cache:

```cpp
mqme::IPacket* packet =
    mqme::IPacket::NewPacket();
```

Give it a destination and some data:

```cpp
packet->SetContext(channel);
packet->SetData('DATA', size, data);
```

and send it:

```cpp
client->SendPacket(packet);
```

or:

```cpp
server->SendPacket(packet);
```

Packets handed to `SendPacket()` are released internally.

Received packets remain valid while your handler is processing them and should be released according to the receive-handler ownership rules.

---

## Direct messages use the same mechanism

Suppose Client A wants to send only to Client B.

Use Client B's identifier as the packet context:

```cpp
packet->SetContext(client_b);
```

Want everyone listening to a shared channel instead?

```cpp
packet->SetContext(channel);
```

Want the packet handled only by the server?

```cpp
packet->SetContext(mqme::NullChannel());
```

Same packet. Same API. Different context.

---

## Building

Clone mqme and configure it with CMake:

```bash
git clone https://github.com/keelanstuart/mqme.git
cd mqme

cmake -S . -B build
cmake --build build --config Release
```

The Pool dependency is retrieved automatically during configuration.

mqme follows a configuration-specific naming convention so the architecture and build type are visible directly in the filename.

Examples on Windows include:

```text
mqme64.dll
mqme64d.dll
mqme64s.lib
mqme64sd.lib
```

Build products are placed in the repository-level `bin` and `lib` directories rather than hidden in configuration-specific directory trees.

---

## What mqme is — and isn't

mqme is intended for applications that need straightforward, efficient communication among a server and a collection of connected clients.

It provides the transport, packet dispatch, threading, and channel routing so your application can concentrate on what the messages mean.

It is **not** intended to be an enterprise message broker, persistent queue, database, RPC framework, or replacement for every networking architecture.

It's the thing you reach for when your problem sounds like:

> **"I have a bunch of programs that need to talk to each other."**

…and you'd rather get on with writing those programs.

---

## License

mqme is open-source software licensed under the GNU Lesser General Public License v3.0.
