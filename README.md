# GTTCan
A time-triggered CAN Bus communication protocol loosely based on TTCan

Each node on the network will need to be able to communicate at 1mbps on the CAN Bus.
The NTU (Network Time Unit) is 0.1us. Each device should maintain its own local time at best-effort resolution. No device on the network should start transmitting until it has received a reference frame to make sure it is synchronised with the other devices.

## Reference Frame

A reference frame is a CAN frame sent with an ID of 1-7 (the first 7 ids are reserved for time masters) with the most significant bit of the 8-byte data payload set to 1. Another bit is kept as a reserved bit, followed by 62 timing bits. 

###### 8 byte (64 bit) payload for reference frame
```
Frtttttt tttttttt tttttttt tttttttt
F = Reference Frame (0/1)
r = reserved
t = timevalue (62 bit)
```

If a time master wishes to transmit other information (not reference frames), the MSB must be set to 0. leaving 63 bits for payload. This does limit the masters transmit range to 0x0 - 0x7FFFFFFFFFFFFFFF. Alternatively, the master may use a different node ID for regular messages. Whilst reference frames should normally be transmitted in the corresponding timeslot in the schedule, it is important that all nodes immediately update their local time upon receiving a reference frame.

## Schedule
The schedule is the heart of the protocol. The exact implementation can be decided by the user, but the following information is required.

*ScheduleLength* - How many entries in one round of the schedule. Int value (recommended <128)\
*WindowTime* - How long (in NTU) each entry in the schedule takes. Must be greater than the tranmission time for a single can frame (in NTU).\

The schedule can then stored as a series of entries (of length *ScheduleLength*). Each entry contains an ID and a data type. For a typical use case, each of these can be encoded as a byte. There are 4 types of entries in the schedule:

#### Reference message slot
ID: 1-7 (depending on which time master is transmitting)\
Data: 0\
A slot for Reference messages to be sent.

#### Exclusive message slot
ID: 8-254 (ID of Node that should transmit in this entry)\
Data: 1-255. \
This is the most common entry in the schedule. The Data value is application specific, but allows for nodes to transmit specific information in a specific schedule entry (and thus one node may appear multiple times in the schedule, but with different data values). If all nodes share a common data structure (similar to an Object Dictionary in CANOpen), this byte could be an index into that data structure. This can also allow all nodes to act as consumers of the messages as they know which node transmitted and what entry should be updated.

#### Arbitration message slot
ID: 255\
Data: 0\
This is a "generic" message slot that allows for ad-hoc or on-request messages to be sent by nodes. In the event that multiple nodes wish to transmit an ad-hoc message, CANBus arbitration will determine which node can transmit in this window.

#### Free Slot
ID: 0\
Data: 0\
This is an empty entry in the schedule that allows for the network to be expanded by adding Exclusive/Arbitration messages

As each entry in the schedule is only 2 bytes, a 64 bit value can be used to pack 4 schedule entries, allowing for a fairly large schedule to be stored in minimal memory. If more entries/nodes are required, there is nothing stopping this 


## Messages


