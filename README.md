# G-TTCan
A time-triggered CAN Bus communication protocol loosely based on TTCan

Each node on the network will need to be able to communicate at 1mbps on the CAN Bus.
The NTU (Network Time Unit) is 0.1us. Each device should maintain its own local time at best-effort resolution. No device on the network should start transmitting until it has received a start of schedule [Reference Message](reference-message) to make sure it is synchronised with the other devices.

## Schedule
The schedule is the heart of the protocol. The exact implementation can be decided by the user, but the following information is required.

*Schedule Length* - How many entries in one round of the schedule. Recommended <128.

*Entry Duration* - How long (in NTU) each entry in the schedule is alloted. Must be greater than the tranmission time for a single can frame (in NTU).

*NUM_ID_BITS* - The number of bits used for Node ID's.

*NUM_DATAID_BITS* - The number of bits used for Data Slot ID's.

*Schedule Data* - The schedule data itself.

The schedule can be stored as a series of entries (of length *Schedule Length*). Each entry contains a node ID and a data ID. For a typical use case, each of these can be encoded as a byte. There are 4 types of entries in the schedule:

#### Reference message slot
ID: 1-7 (depending on which time master is transmitting)\
Data: 0\
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

All messages should be sent using extended can frame ID's. This allows for a 29bit ID field to be transmitted with each frame, which is used to transmit both the node id and the data field id.

The bit formatting is as follows:
```
0 x (29-NUM_ID_BITS-NUM_DATAID_BITS)
i x NUM_ID_BITS
d x NUM_DATAID_BITS

i = Node ID value
d = Data ID value

For an 8 bit Node ID and a 16bit data slot ID, the CAN Frame ID value would look like this:
00000iiiiiiiidddddddddddddddd
```

#### Reference message

A reference message is a CAN frame sent by a node with an ID of 1-7 (the first 7 ids are reserved for time masters) with a data ID of 0. The first bit is used to indicate that this reference frame is the first frame in the schedule. This allows for the schedule to contain multiple reference messages for tighter time synchronisation. This is followed by 62 timing bits. 


##### 8 byte (64 bit) data payload for reference messages
```
SRtttttt tttttttt tttttttt tttttttt
S = Start of schedule (0/1)
R = reserved
t = timevalue (62 bit)
```

<!--
#### Exclusive Message

This is a standard message. Although the schedule slot dictates which node and what data will be sent, the Data ID value should still be encoded in the least-significant bits in the Can Frame ID. All 8 data bytes are available for data transmission.

#### Arbitration message

This is a message slot for any node to send unscheduled messages in. It is the same as a Data message, however the transmitting node will need to deal with a potential loss of arbitration and attempt to send the message again in the next arbitration slot.
-->

## Nodes

Each node on the G-TTCan network will need the following information:

*Node ID(s)*: The ID(s) to be used for this node. Node ID's 1-7 are reserved for time masters. It is *possible* for a node to identify/respond as 2 IDs (schizophrenic node) however this may introduce unexpected complexity or behaviour and should be carefully considered.

*Current Time*: The current network time (in units of 0.1 us). This should be syncrhonised when receiving a [Reference Message](reference-message) and maintained at best-effort resolution by the node.

### Masters

Node's with an ID of 1 - 7 are potential master nodes.

Primary Master (ID: 1): This node should 

