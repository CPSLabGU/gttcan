# G-TTCan
A time-triggered CAN Bus communication protocol loosely based on TTCan

Each node on the network will need to be able to communicate at 1mbps on the CAN Bus.
The NTU (Network Time Unit) is 0.1us. Each device should maintain its own local time at best-effort resolution. No device on the network should start transmitting until it has received a start of schedule [Reference Message](reference-message) to make sure it is synchronised with the other devices.

## Schedule
The schedule is the basis of the protocol. The exact implementation can be decided by the user, but the following information is required.

*Schedule Length* - How many entries in one round of the schedule.

*Entry Duration* - How long (in NTU) each entry in the schedule is alloted. Must be greater than the tranmission time for a single can frame (in NTU).

*Schedule Data* - The schedule data itself. This does not need to be stored on each node, or anywhere even, however the information from the schedule must be used to generate each node's *local schedule*. In practice, it may be desirable for every node to have the full schedule.

The schedule can be stored as a series of entries (of length *Schedule Length*). Each entry contains a node ID and a data ID. For a typical use case, each of these can be encoded as a byte, though for larger networks/data structures, these could be encoded as 2 bytes each. There are 4 types of entries in the schedule:

#### Reference message slot
Node ID: 1-7 (depending on which time master is transmitting)\
Data ID: 0\
A slot for Reference messages to be sent.

#### Exclusive message slot
ID: 1 - (2^NUM_ID_BITS)-1 (ID of Node that should transmit in this slot)\
DataID: 1 - (2^NUM_DATAID_BITS)-1. \
This is the most common entry in the schedule. The Data value is application specific, but allows for nodes to transmit specific information in a specific schedule entry (and thus one node may appear multiple times in the schedule, but with different data values). If all nodes share a common data structure (similar to an Object Dictionary in CANOpen), this number could be an index into that data structure. This can also allow all nodes to act as consumers of the messages as they know which node transmitted and what entry should be updated.

#### Arbitration message slot
ID: 2^NUM_ID_BITS\
DataID: 2^NUM_DATAID_BITS\
This is a "generic" message slot that allows for ad-hoc or on-request messages to be sent by nodes. In the event that multiple nodes wish to transmit an ad-hoc message, CANBus arbitration will determine which node can transmit in this window. IF a node loses arbitration, it can attempt to send again in the next arbitration message slot.

#### Free Slot
ID: 0\
Data: 0\
This is an empty entry in the schedule that allows for the network to be expanded by adding Exclusive/Arbitration messages

It is important to note that only 29 bits are available in the CAN Frame header id field, so NUM_ID_BITS + NUM_DATAID_BITS must be <= 29.


## Messages

#### 29 bit ID for CAN Frame Header

All messages should be sent using extended can frame ID's. This allows for a 29bit ID field to be transmitted with each frame, which is used to transmit both the schedule index and the data field id.

The bit formatting is as follows:
```
0 x (29-NUM_ID_BITS-NUM_DATAID_BITS)
i x SCHEDULE_ID_BITS
d x NUM_DATAID_BITS

i = Schedule index value
d = Data ID value

For an 14 bit schedule index and a 14 bit data slot ID, the CAN Frame ID value would look like this:
0iiiiiiiiiiiiiidddddddddddddd
```

#### Reference message

A reference message is a CAN frame sent by a node with an ID of 1-7 (the first 7 ids are reserved for time masters) with a data ID of 0. The first data bit (MSB) is used to indicate that this reference frame is the first frame in the schedule. This allows for the schedule to contain multiple reference messages for tighter time synchronisation. This is followed by a reserved bit and then 62 timing bits. 


##### 8 byte (64 bit) Data payload for reference messages
```
SRtttttt tttttttt tttttttt tttttttt
S = Start of schedule (0/1)
R = reserved
t = timevalue (62 bit)
```


#### Standard Message

A standard message has the full 8 bytes available for data transmission. Although the schedule slot dictates what data will be sent, the Data ID value should still be encoded in the least-significant bits in the Can Frame ID. 

<!--
#### Arbitration message

This is a message slot for any node to send unscheduled messages in. It is the same as a Data message, however the transmitting node will need to deal with a potential loss of arbitration and attempt to send the message again in the next arbitration slot.
-->

## Nodes

Each node on the G-TTCan network will need the following information:

*Node ID(s)*: The ID(s) to be used for this node. Node ID's 1-7 are reserved for time masters. It is *possible* for a node to identify/respond as 2 IDs (schizophrenic node) however this may introduce unexpected complexity or behaviour and should be carefully considered.

*Current Time*: The current network time (in units of 0.1 us). This should be syncrhonised when receiving a [Reference Message](reference-message) and maintained at best-effort resolution by the node.

*Schedule Length* - How many entries in one round of the schedule.

*Entry Duration* - How long (in NTU) each entry in the schedule is alloted. Must be greater than the tranmission time for a single can frame (in NTU).

*Local Schedule* - Each node will need to know when it should transmit in the schedule. This can be stored as a series of <global schedule index - data slot id> pairs (and this will fit inside a single uint32). Each node can calculate the next transmission time by subtracting the current global schedule index from it's next transmission index and multiplying this result (the number of schedule slots until a node must next transmit) by the slot duration.

### Masters (TBC)

Node's with an ID of 1 - 7 are potential master nodes.
Primary Master (ID: 1): This node should 

