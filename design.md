# Protocol Design and Implementation

This document describes the design and implementation of the two replication protocols used in this distributed key-value store: the **Blocking Protocol** and the **ABD Protocol**.


## Blocking Protocol

### Overview

The Blocking Protocol uses **per-key locks** to coordinate access to the key-value store. Clients must acquire locks before performing read or write operations, ensuring exclusive access to keys during operations.

### Key Characteristics

- **Lock-based coordination**: Each key has an associated lock
- **May block**: Clients can block waiting for locks held by other clients
- **Client failure impact**: If a client crashes while holding a lock, other clients may be blocked until the lock times out (30 seconds)
- **Linearizable**: All operations appear to execute atomically in some global order
- **Quorum-based**: Uses read quorum (R) and write quorum (W) for replication

### Algorithm Flow

#### Write Operation (3 Servers, Quorum W=2)

```mermaid
flowchart TD
    Client["Client<br/><br/>Generate Value<br/>Prepare Write"]
    Acquire["1. Acquire Locks<br/>(Need Quorum W=2<br/>from 3 servers)"]
    
    S1["Server 1<br/>Port 5001<br/><br/>Lock Request"]
    S2["Server 2<br/>Port 5001<br/><br/>Lock Request"]
    S3["Server 3<br/>Port 5001<br/><br/>Lock Request<br/>(Optional)"]
    
    Lock1["Lock Granted"]
    Lock2["Lock Granted"]
    
    Write1["2. Write Value<br/>to Server 1"]
    Write2["2. Write Value<br/>to Server 2"]
    
    Release1["3. Release Lock"]
    Release2["3. Release Lock"]
    
    Quorum["Quorum W=2 achieved <br/>(Only need 2 out of 3)"]
    Success["Success<br/>Write Complete"]
    
    Client --> Acquire
    Acquire --> S1
    Acquire --> S2
    Acquire -.->|Optional| S3
    
    S1 --> Lock1
    S2 --> Lock2
    
    Lock1 --> Write1
    Lock2 --> Write2
    
    Write1 --> Release1
    Write2 --> Release2
    
    Release1 --> Quorum
    Release2 --> Quorum
    
    Quorum --> Success
    
    style Client fill:#7BB3F0,stroke:#4A90E2,stroke-width:2px,color:#fff
    style Acquire fill:#FFC966,stroke:#F5A623,stroke-width:2px,color:#000
    style S1 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S2 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S3 fill:#E0E0E0,stroke:#B0B0B0,stroke-width:2px,color:#000
    style Lock1 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Lock2 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Write1 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Write2 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Release1 fill:#D4F5B0,stroke:#B8E986,stroke-width:2px,color:#000
    style Release2 fill:#D4F5B0,stroke:#B8E986,stroke-width:2px,color:#000
    style Quorum fill:#FFF066,stroke:#F8E71C,stroke-width:2px,color:#000
    style Success fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
```

**Steps:**
1. **Acquire Locks**: Client requests locks from all servers. Needs write quorum (W) locks to proceed.
2. **Write Value**: Once locks are acquired, write the value to all locked servers.
3. **Release Locks**: Release all acquired locks.

#### Read Operation (3 Servers, Quorum R=2)

```mermaid
flowchart TD
    Client["Client<br/><br/>Prepare Read"]
    Acquire["1. Acquire Locks<br/>(Need Quorum R=2<br/>from 3 servers)"]
    
    S1["Server 1<br/>Port 5001<br/><br/>Lock Request"]
    S2["Server 2<br/>Port 5001<br/><br/>Lock Request"]
    S3["Server 3<br/>Port 5001<br/><br/>Lock Request<br/>(Optional)"]
    
    Lock1["Lock Granted"]
    Lock2["Lock Granted"]
    
    Read1["2. Read Value<br/>ts=100<br/>val='A'"]
    Read2["2. Read Value<br/>ts=150<br/>val='B'"]
    
    Release1["3. Release Lock"]
    Release2["3. Release Lock"]
    
    FindMax["4. Find Max Timestamp<br/>Comparing: ts=100, ts=150<br/>Max: ts=150, val='B'"]
    Return["Return 'B'<br/>(ts=150)"]
    
    Client --> Acquire
    Acquire --> S1
    Acquire --> S2
    Acquire -.->|Optional| S3
    
    S1 --> Lock1
    S2 --> Lock2
    
    Lock1 --> Read1
    Lock2 --> Read2
    
    Read1 --> Release1
    Read2 --> Release2
    
    Release1 --> FindMax
    Release2 --> FindMax
    
    FindMax --> Return
    
    style Client fill:#7BB3F0,stroke:#4A90E2,stroke-width:2px,color:#fff
    style Acquire fill:#FFC966,stroke:#F5A623,stroke-width:2px,color:#000
    style S1 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S2 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S3 fill:#E0E0E0,stroke:#B0B0B0,stroke-width:2px,color:#000
    style Lock1 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Lock2 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Read1 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Read2 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Release1 fill:#D4F5B0,stroke:#B8E986,stroke-width:2px,color:#000
    style Release2 fill:#D4F5B0,stroke:#B8E986,stroke-width:2px,color:#000
    style FindMax fill:#FFF066,stroke:#F8E71C,stroke-width:2px,color:#000
    style Return fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
```

**Steps:**
1. **Acquire Locks**: Client requests locks from all servers. Needs read quorum (R) locks.
2. **Read Values**: Read values and timestamps from all locked servers.
3. **Find Maximum**: Select the value with the highest timestamp.
4. **Release Locks**: Release all acquired locks.

### Lock Management

- **Lock Timeout**: Locks expire after 30 seconds of inactivity
- **Re-entrant**: Same client can acquire the same lock multiple times
- **Overtaking**: If a lock times out, another client can acquire it
- **Per-Key**: Each key has its own independent lock

### Implementation Details

**Server-Side:**
- Maintains a lock table: `std::map<std::string, LockEntry>`
- Each `LockEntry` tracks: owner client ID, acquisition time
- Lock acquisition checks: unlocked, timed out, or same client
- Read/Write operations verify lock ownership

**Client-Side:**
- Requests locks from all servers in parallel
- Waits for quorum (R or W) locks before proceeding
- Releases locks after operation completes
- Handles lock acquisition failures gracefully


### Disadvantages

- **May Block**: Clients can wait indefinitely for locks
- **Client Failure Impact**: Crashed clients can block others for up to 30 seconds
- **Lower Availability**: Lock contention reduces system throughput
- **Deadlock Potential**: Multiple keys can lead to deadlocks (mitigated by timeouts)

---

## ABD Protocol

### Overview

The ABD Protocol is a **non-blocking, wait-free** replication protocol that uses **quorum-based reads and writes** with **timestamp ordering** to achieve linearizability without locks.

### Key Characteristics

- **Non-blocking**: Clients never block waiting for slow or failed servers
- **Wait-free**: Operations complete as soon as quorum is achieved
- **Quorum-based**: Uses read quorum (R) and write quorum (W)
- **Timestamp ordering**: Uses logical timestamps to order operations
- **Linearizable**: All operations appear to execute atomically
- **Resilient to client failures**: Client crashes don't affect other clients

### Algorithm Flow

#### Write Operation (3 Servers, Quorum W=2)

```mermaid
flowchart TD
    Client["Client<br/><br/>Generate Timestamp<br/>(ts=200)"]
    WriteQuorum["1. Write to Quorum<br/>(Need W=2 from 3 servers)"]
    
    S1["Server 1<br/>Port 5001<br/><br/>Write Request<br/>(ts=200)"]
    S2["Server 2<br/>Port 5001<br/><br/>Write Request<br/>(ts=200)"]
    S3["Server 3<br/>Port 5001<br/><br/>Write Request<br/>(ts=200)<br/>(Optional)"]
    
    Store1["Store Value<br/>Assign ts≥200"]
    Store2["Store Value<br/>Assign ts≥200"]
    
    ACK1["ACK<br/>ts=200"]
    ACK2["ACK<br/>ts=200"]
    
    Quorum["Quorum W=2<br/>acknowledged <br/>(Only need 2 out of 3)"]
    Success["Success<br/>Write Complete"]
    
    Client --> WriteQuorum
    WriteQuorum --> S1
    WriteQuorum --> S2
    WriteQuorum -.->|Optional| S3
    
    S1 --> Store1
    S2 --> Store2
    
    Store1 --> ACK1
    Store2 --> ACK2
    
    ACK1 --> Quorum
    ACK2 --> Quorum
    
    Quorum --> Success
    
    style Client fill:#7BB3F0,stroke:#4A90E2,stroke-width:2px,color:#fff
    style WriteQuorum fill:#FFC966,stroke:#F5A623,stroke-width:2px,color:#000
    style S1 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S2 fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S3 fill:#E0E0E0,stroke:#B0B0B0,stroke-width:2px,color:#000
    style Store1 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Store2 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style ACK1 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style ACK2 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Quorum fill:#FFF066,stroke:#F8E71C,stroke-width:2px,color:#000
    style Success fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
```

**Steps:**
1. **Generate Timestamp**: Client generates a unique timestamp (monotonically increasing)
2. **Write to Quorum**: Send write request to all servers, wait for write quorum (W) acknowledgments
3. **Complete**: Operation succeeds once W servers acknowledge

#### Read Operation (Two-Phase, 3 Servers, R=2, W=2)

```mermaid
flowchart TD
    Client["Client<br/><br/>Prepare Read"]
    Phase1["PHASE 1: READ<br/>Need Quorum R=2<br/>from 3 servers"]
    
    S1_Read["Server 1<br/>Port 5001<br/><br/>Read Request"]
    S2_Read["Server 2<br/>Port 5001<br/><br/>Read Request"]
    S3_Read["Server 3<br/>Port 5001<br/><br/>Read Request<br/>(Optional)"]
    
    Resp1["Response<br/>ts=100<br/>val='A'"]
    Resp2["Response<br/>ts=150<br/>val='B'"]
    
    FindMax["Find Max Timestamp<br/>Max: ts=150<br/>val='B'<br/><br/>Generate ts=151"]
    
    Phase2["PHASE 2: WRITE-BACK<br/>Need Quorum W=2<br/>from 3 servers"]
    
    S1_Write["Server 1<br/>Port 5001<br/><br/>Write Request<br/>(ts=151, val='B')"]
    S2_Write["Server 2<br/>Port 5001<br/><br/>Write Request<br/>(ts=151, val='B')"]
    S3_Write["Server 3<br/>Port 5001<br/><br/>Write Request<br/>(ts=151, val='B')<br/>(Optional)"]
    
    Store1["Store Value<br/>Assign ts≥151"]
    Store2["Store Value<br/>Assign ts≥151"]
    
    ACK1["ACK<br/>ts=151"]
    ACK2["ACK<br/>ts=151"]
    
    Quorum["Quorum W=2<br/>written <br/>(Only need 2 out of 3)"]
    Return["Return 'B'<br/>(ts=151)"]
    
    Client --> Phase1
    Phase1 --> S1_Read
    Phase1 --> S2_Read
    Phase1 -.->|Optional| S3_Read
    
    S1_Read --> Resp1
    S2_Read --> Resp2
    
    Resp1 --> FindMax
    Resp2 --> FindMax
    
    FindMax --> Phase2
    
    Phase2 --> S1_Write
    Phase2 --> S2_Write
    Phase2 -.->|Optional| S3_Write
    
    S1_Write --> Store1
    S2_Write --> Store2
    
    Store1 --> ACK1
    Store2 --> ACK2
    
    ACK1 --> Quorum
    ACK2 --> Quorum
    
    Quorum --> Return
    
    style Client fill:#7BB3F0,stroke:#4A90E2,stroke-width:2px,color:#fff
    style Phase1 fill:#FFC966,stroke:#F5A623,stroke-width:2px,color:#000
    style Phase2 fill:#FFC966,stroke:#F5A623,stroke-width:2px,color:#000
    style S1_Read fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S2_Read fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S3_Read fill:#E0E0E0,stroke:#B0B0B0,stroke-width:2px,color:#000
    style Resp1 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Resp2 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style FindMax fill:#FFF066,stroke:#F8E71C,stroke-width:2px,color:#000
    style S1_Write fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S2_Write fill:#A5E85C,stroke:#7ED321,stroke-width:2px,color:#000
    style S3_Write fill:#E0E0E0,stroke:#B0B0B0,stroke-width:2px,color:#000
    style Store1 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style Store2 fill:#E87DFF,stroke:#BD10E0,stroke-width:2px,color:#fff
    style ACK1 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style ACK2 fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
    style Quorum fill:#FFF066,stroke:#F8E71C,stroke-width:2px,color:#000
    style Return fill:#7FEDD4,stroke:#50E3C2,stroke-width:2px,color:#000
```

**Steps:**
1. **Phase 1 - Read from Quorum**: 
   - Send read requests to all servers
   - Collect responses from read quorum (R) servers
   - Each response contains: value, timestamp
   
2. **Find Maximum Timestamp**:
   - Select the value with the highest timestamp
   - This ensures we get the most recent value
   
3. **Phase 2 - Write-Back**:
   - Write the maximum value back to write quorum (W) servers
   - Use a timestamp greater than the maximum seen
   - This ensures all servers converge to the latest value



The two-phase read is necessary because:
- **Phase 1** may return stale values from some servers
- **Phase 2** propagates the latest value to all servers
- This ensures **linearizability**: all subsequent reads see at least this value

### Timestamp Management

- **Client Timestamps**: Each client maintains a logical clock
- **Monotonicity**: Client timestamps always increase
- **Server Timestamps**: Servers assign timestamps ≥ client timestamps
- **Ordering**: Operations are ordered by their timestamps

### Implementation Details

**Server-Side:**
- Maintains key-value store: `std::map<std::string, ValueEntry>`
- Each `ValueEntry` contains: value, timestamp
- Read returns current value and timestamp
- Write accepts value and client timestamp, assigns server timestamp ≥ client timestamp

**Client-Side:**
- Maintains logical clock (monotonically increasing)
- Read: Two-phase (read quorum → find max → write-back to write quorum)
- Write: Single-phase (write to write quorum)
- Never blocks: Proceeds as soon as quorum is achieved


### Disadvantages

- **Read Overhead**: Two-phase reads (read + write-back) are more expensive
- **Network Messages**: More RPC calls per read operation
- **Complexity**: More complex than simple locking

---

## Implementation Details

### Quorum Configuration

For **N servers**, quorum sizes must satisfy:
- **R + W > N**: Ensures read and write quorums overlap (guarantees consistency)
- **W > N/2**: Ensures write quorum is majority (prevents split-brain)

**Common configurations:**
- **3 servers**: R=2, W=2 (can tolerate 1 failure)
- **5 servers**: R=3, W=3 (can tolerate 2 failures)

### Consistency Guarantees

Both protocols guarantee **linearizability**:
- All operations appear to execute atomically
- There exists a global ordering of all operations
- Each read returns the value written by the most recent write in that ordering

### Performance Characteristics

**Blocking Protocol:**
- **Read**: Lock acquisition + Read + Lock release
- **Write**: Lock acquisition + Write + Lock release
- **Latency**: Higher due to lock coordination
- **Throughput**: Lower due to lock contention

**ABD Protocol:**
- **Read**: Two-phase (read quorum + write-back to write quorum)
- **Write**: Single-phase (write to write quorum)
- **Latency**: Lower for writes, higher for reads (due to write-back)
- **Throughput**: Higher due to non-blocking nature

---

## Summary

Both protocols provide linearizability but make different trade-offs:
- **Blocking Protocol**: Simpler, but may block and is vulnerable to client failures
- **ABD Protocol**: More complex, but non-blocking and resilient to client failures

