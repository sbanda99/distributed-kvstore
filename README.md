# Distributed Key-Value Store with Linearizable Replication Protocols

A distributed key-value store implementation in C++ that provides linearizability guarantees using two different replication protocols: **ABD Protocol** and **Blocking Protocol**. This project demonstrates the trade-offs between availability and consistency in distributed systems.

## Overview

This project implements a replicated key-value store with the following features:

- **Linearizability Guarantee**: All operations appear to execute atomically in some global order
- **Two Replication Protocols**:
  - **ABD Protocol** : Non-blocking, wait-free, quorum-based
  - **Blocking Protocol**: Lock-based coordination with per-key locks
- **gRPC-based Communication**: Client-server communication using Protocol Buffers
- **Performance Evaluation**: Tools for measuring throughput, latency, and crash impact

For detailed information about how the ABD and Blocking protocols work, their algorithms, and implementation details, please refer to the **[design.md](design.md)** file.

## Experimental Setup

We used conda environment to download the dependencies for the project. As we don't have sudo access to install dependencies on W135 machines.

We used the scratch space for installing conda. Please update the paths to install conda accordingly.

### Step 1: Setup Conda Environment

On each machine where you'll run servers or clients:

```bash
# Install Miniconda (if not already installed)
mkdir -p $SCRATCH_PATH$/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O $SCRATCH_PATH$/miniconda3/miniconda.sh
bash $SCRATCH_PATH$/miniconda3/miniconda.sh -b -u -p /scratch/sqb6440/miniconda3

# Create conda environment
conda create -n mapreduce -y python=3.9
conda activate mapreduce

# Install required packages
conda install -y -c conda-forge grpc-cpp protobuf protobuf-compiler
```

### Step 2: Build the Project

```bash
# Clone or navigate to the project directory
cd /path/to/kvstore_project

# Activate conda environment
conda activate mapreduce

# Build all binaries
make clean
make
```

This will create the following executables in the `build/` directory:
- `abd_server` / `blocking_server` - Server executables
- `abd_client` / `blocking_client` - Client executables
- `test_correctness_abd` / `test_correctness_blocking` - Correctness test suites
- `evaluate_performance` - Performance evaluation tool
- `evaluate_crash_impact` - Crash impact evaluation tool

## Configuration Files

Configuration files are located in the `config/` directory. Each file specifies:
- Server addresses (hostnames or IPs)
- Protocol type (ABD or Blocking)
- Quorum sizes (read quorum R, write quorum W)
- Number of replicas

**Available configurations:**
- `config_1server_abd.json` / `config_1server_blocking.json` - Single server
- `config_3servers_abd.json` / `config_3servers_blocking.json` - Three servers
- `config_5servers_abd.json` / `config_5servers_blocking.json` - Five servers
- `config_localhost_3servers.json` - Three servers on localhost (for local testing)

**To update server addresses**, edit the config files and replace hostnames/IPs:
```json
{
  "servers": [
    {"id": 0, "host": "e5-cse-135-07.cse.psu.edu", "port": 5001},
    {"id": 1, "host": "e5-cse-135-08.cse.psu.edu", "port": 5001},
    {"id": 2, "host": "e5-cse-135-09.cse.psu.edu", "port": 5001}
  ],
  ...
}
```

## Running the System

### Starting Servers

On each server machine, start the appropriate server:

**ABD Protocol:**
```bash
# On machine 1 (Server ID 0)
./build/abd_server --port 5001 --server-id 0

# On machine 2 (Server ID 1)
./build/abd_server --port 5001 --server-id 1

# On machine 3 (Server ID 2)
./build/abd_server --port 5001 --server-id 2
```

**Blocking Protocol:**
```bash
# On machine 1 (Server ID 0)
./build/blocking_server --port 5001 --server-id 0

# On machine 2 (Server ID 1)
./build/blocking_server --port 5001 --server-id 1

# On machine 3 (Server ID 2)
./build/blocking_server --port 5001 --server-id 2
```

### Running Clients

**Interactive Mode:**
```bash
# ABD client
./build/abd_client config/config_3servers_abd.json

# Blocking client
./build/blocking_client config/config_3servers_blocking.json
```

**Direct Commands:**
```bash
# Write operation
./build/abd_client config/config_3servers_abd.json write mykey "myvalue"

# Read operation
./build/abd_client config/config_3servers_abd.json read mykey
```

### Running Correctness Tests

```bash
# Test ABD protocol correctness
./build/test_correctness_abd config/config_3servers_abd.json

# Test Blocking protocol correctness
./build/test_correctness_blocking config/config_3servers_blocking.json
```

### Performance Evaluation

**Basic Performance Test:**
```bash
# Syntax: <config> <protocol> <num_clients> <get_ratio> <duration_sec>
./build/evaluate_performance config/config_3servers_abd.json abd 10 0.9 60
```

**Finding Saturation Point:**
```bash
# Automated script to test with increasing client counts
./find_saturation.sh config/config_3servers_abd.json abd 0.9
```

**Crash Impact Evaluation:**
```bash
# Syntax: <config> <protocol> <num_clients> <crash_after_sec> <total_duration_sec>
./build/evaluate_crash_impact config/config_3servers_abd.json abd 5 10 30
```


## Protocol Comparison
Below is my understanding of both the protocols from this project.

| Feature | ABD Protocol | Blocking Protocol |
|---------|-------------|-------------------|
| **Blocking** | Non-blocking, wait-free | May block on lock acquisition |
| **Client Failures** | Resilient (no impact) | Can block other clients |
| **Consistency** | Linearizable | Linearizable |
| **Availability** | High (quorum-based) | Lower (lock-based) |
| **Read Complexity** | Two-phase (read + write-back) | Lock + read + unlock |
| **Write Complexity** | Single-phase (quorum write) | Lock + write + unlock |

For detailed protocol descriptions and flow diagrams, see **[design.md](design.md)**.

