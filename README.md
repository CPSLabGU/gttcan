# G-TTCan
A time-triggered CAN Bus communication protocol loosely based on TTCan

Each node on the network will need to be able to communicate at 1mbps on the CAN Bus.
The NTU (Network Time Unit) is 0.1us. Each device should maintain its own local time at best-effort resolution. No device on the network should start transmitting until it has received a start of schedule [Reference Message](reference-message) to make sure it is synchronised with the other devices.

## Schedule
The schedule is the heart of the protocol. The exact implementation can be decided by the user, but the following information is required.

*Schedule Length* - How many entries in one round of the schedule. Recommended <128.

*Entry Duration* - How long (in NTU) each entry in the schedule is alloted. Must be greater than the tranmission time for a single can frame (in NTU).

*Schedule Data* - The schedule data itself.

The schedule can be stored as a series of entries (of length *Schedule Length*). Each entry contains an ID and a data type. For a typical use case, each of these can be encoded as a byte. There are 4 types of entries in the schedule:

#### Reference message slot
ID: 1-7 (depending on which time master is transmitting)\
Data: 0\
A slot for Reference messages to be sent.

#### Exclusive message slot
ID: 1-254 (ID of Node that should transmit in this slot)\
Data: 1-254. \
This is the most common entry in the schedule. The Data value is application specific, but allows for nodes to transmit specific information in a specific schedule entry (and thus one node may appear multiple times in the schedule, but with different data values). If all nodes share a common data structure (similar to an Object Dictionary in CANOpen), this number could be an index into that data structure. This can also allow all nodes to act as consumers of the messages as they know which node transmitted and what entry should be updated.

#### Arbitration message slot
ID: 255\
Data: 255\
This is a "generic" message slot that allows for ad-hoc or on-request messages to be sent by nodes. In the event that multiple nodes wish to transmit an ad-hoc message, CANBus arbitration will determine which node can transmit in this window. IF a node loses arbitration, it can attempt to send its [Arbitration Message](arbitration-message) again in the next arbitration message slot.

#### Free Slot
ID: 0\
Data: 0\
This is an empty entry in the schedule that allows for the network to be expanded by adding Exclusive/Arbitration messages

As each entry in the schedule is only 2 bytes, a 64 bit value can be used to pack 4 schedule entries, allowing for a fairly large schedule to be stored in minimal memory. If more entries/nodes are required, there is nothing stopping the id and data fields from being expanded, however as the ID and the Data field are sent in the CAN Frame ID bits, they are limited (combined) to 29 bits.


## Messages

#### Reference message

A reference message is a CAN frame sent by a node with an ID of 1-7 (the first 7 ids are reserved for time masters) with the most significant bit of the 8-byte data payload set to 1. The next bit is used to indicate that this reference frame is the first frame in the schedule. This allows for the schedule to contain multiple reference messages for tighter time synchronisation. This is followed by 62 timing bits. 

##### 29 bit ID for CAN Frame Header - Reference Message
```
0000000000000iiiiiiii00000000 (recommended)
i = Node ID (8 bit) - values 1-7
```
##### 8 byte (64 bit) data payload for reference messages
```
FStttttt tttttttt tttttttt tttttttt
F = Reference Frame (0/1)
S = Start of schedule (0/1)
t = timevalue (62 bit)
```

If a time master wishes to transmit other information (not reference messages), the MSB must be set to 0, leaving 63 bits for payload. This does limit the masters transmit range to 0x0 - 0x7FFFFFFFFFFFFFFF. Alternatively, the master may use a different  ID for regular (non Reference) messages. Whilst reference messages should normally be transmitted in the corresponding timeslot in the schedule, it is important that all nodes immediately update their local time upon receiving a reference message at ANY time.

#### Exclusive Message

This is a standard message. Although the schedule slot dictates which node and what data will be sent, the Data field should still be encoded in the least-significant bits in the Can Frame ID

##### 29 bit ID for CAN Frame Header
```
0000000000000iiiiiiiidddddddd (recommended)
i = Node ID (8 bit) - values 1-254
d = Data OD (8 bit) - value 1-254
```

#### Arbitration message

This is a message for a nodes response to a [Request Message](request-message). It is the same as a Data message, however the Data field byte will not exist in the schedule, so needs to be transmitted as part of the Can Frame ID. In order for this to be possible, extended CAN frames must be used, allowing up to 29bits to be transmitted in the CAN Frame ID.

> Gervase: Unlike other exclusive message slots, the type of data being transmitted in this message is not contained in the schedule. Do we want to include the datafield byte in the ID field (given we have many unused bits). We are only using 8 bit IDs, but have 28 bits available, so we could easily use the next 8 bits in the ID field to specify a data byte. It would mean arbitration would be decided by data field and not by id.

## Nodes

Each node on the G-TTCan network will need the following information:

*Node ID(s)*: The ID(s) to be used for this node. Node ID's 1-7 are reserved for time masters. It is *possible* for a node to identify/respond as 2 IDs (schizophrenic node) however this may introduce unexpected complexity or behaviour and should be carefully considered.

*Current Time*: The current network time (in units of 0.1 us). This should be syncrhonised when receiving a [Reference Message](reference-message) and maintained at best-effort resolution by the node.

### Masters

Node's with an ID of 1 - 7 are potential master nodes.

Primary Master (ID: 1): This node should 

