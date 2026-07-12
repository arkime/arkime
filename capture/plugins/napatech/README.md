# Arkime Napatech Reader Plugin

Packet capture reader for Napatech adapters using the NTAPI (libntapi) SDK.
Tested on **NT200A02** with Link-Capture Software 12.x and Arkime 6.x.

## Requirements

| Component | Version |
|-----------|---------|
| Napatech Link-Capture Software | 12.x (ntanl3) |
| libntapi / libntapi-dev | Installed with Link-Capture Software |
| Arkime | 6.x |
| libpcap-dev | Any recent version |

The Napatech kernel module (`nt3gd`) must be loaded and `ntservice` must be
running before Arkime starts.

---

## Build

```sh
cd capture/plugins/napatech
make
sudo cp reader-napatech.so /opt/arkime/plugins/
```

The `Makefile` detects libpcap headers via `pcap-config` or `pkg-config libpcap`
automatically. To override the Napatech install path:

```sh
make NTDIR=/path/to/napatech3
```

---

## ntservice Setup

`ntservice` controls the DMA ring buffers the FPGA writes captured packets
into. It must be configured before starting Arkime.

### `/etc/opt/napatech3/ntservice.ini` (minimum required sections)

```ini
[System]
TimestampFormat = UNIX_NS       # Required: plugin reads UNIX_NANOTIME descriptors

[Adapter0]
BusId = 0000:b8:00.0            # PCIe bus ID (find with: lspci | grep Napatech)
AdapterType = NT200A02_8X10     # Replace with your card type

# 8 RX host buffers x 2048 MB on NUMA node 1 (card-local memory)
# Reduce count or size to use less RAM; these are locked DMA allocations.
HostBuffersRx = [8, 2048, 1]
HostBuffersTx = [0, 16, 1]
```

`HostBuffersRx = [count, size_MB, NUMA_node]` — this memory is allocated
**at ntservice startup** from locked (non-swappable) RAM. With `[8, 2048, 1]`
that is 16 GB. Reduce `size_MB` or `count` if memory is limited.

Start ntservice:

```sh
sudo /opt/napatech3/bin/ntservice -d -f /etc/opt/napatech3/ntservice.ini
```

---

## NTPL Stream Assignment

NTPL rules tell the FPGA which ports to capture and which stream IDs to
route traffic to. These must be applied before Arkime starts, **or** set
`ntplFile=` in `config.ini` to have the plugin apply them automatically at
startup.

### Key NTPL requirements

```ntpl
Delete = All                    # Clear any previous rules

# Sorted 5-tuple hash: both directions of each TCP/UDP flow land on the
# same stream ID, which may improve Arkime's session reassembly.
HashMode[Priority=0; Layer3Type=IP; Layer4Type=TCP,UDP,SCTP] = Hash5TupleSorted
HashMode[Priority=1; Layer3Type=IP]                           = Hash2TupleSorted

# Distribute ports across streams (adjust to your port/stream layout)
Assign[Priority=0; StreamId=(0..3)] = port == 0 or port == 4
Assign[Priority=0; StreamId=(4..7)] = port == 1 or port == 2 or port == 5 or port == 6
Assign[Priority=0; StreamId=drop]   = port == 3 or port == 7
```

See `arkime-streams.ntpl` for a complete example with an 8x10G NT200A02.

Apply manually:

```sh
sudo /opt/napatech3/bin/ntpl -f arkime-streams.ntpl
```

Or set `ntplFile=` in `config.ini` to apply automatically at each startup.

> **`Hash5TupleSorted` vs `Hash5Tuple`**: the sorted variant normalizes the
> 5-tuple (lower IP first) before hashing, so forward and reverse packets of
> the same flow always land on the same stream.

---

## Arkime `config.ini` Settings

```ini
interface      = none
pcapReadMethod = napatech
rootPlugins    = reader-napatech.so
```

### Optional settings

| Setting | Default | Description |
|---------|---------|-------------|
| `ntplFile` | _(none)_ | Path to an NTPL file to apply at startup. Applied after `NT_Init()`, before streams are opened. If not set, NTPL must be applied manually. |
| `ntStreamIds` | _(auto)_ | Comma-separated stream ID list/ranges to open, e.g. `0-7` or `0-3,5,8-10`. If not set, the plugin queries the driver for all stream IDs that have host buffers allocated (`NT_INFO_CMD_READ_STREAM`) and opens all of them. |

#### `ntStreamIds` format examples

```ini
ntStreamIds = 0-7          # streams 0, 1, 2, 3, 4, 5, 6, 7
ntStreamIds = 0-3,5        # streams 0, 1, 2, 3, 5
ntStreamIds = 0-3,8-10     # streams 0, 1, 2, 3, 8, 9, 10
```

Use this to open a specific subset of streams when you do not want Arkime to
consume all configured streams (e.g. when multiple consumers share the buffers).

---

## How It Works

### Privilege drop

Arkime drops privileges to `dropUser` immediately after calling the reader's
`init()` callback. `NT_Init()` (which opens `/dev/nt3gd`) and all
`NT_NetRxOpen()` calls are in `reader_napatech_init()`, which runs **as
root** before the privilege drop. The capture threads start later, already
as `dropUser`.

### Zero-copy capture

The plugin uses `NT_NET_INTERFACE_SEGMENT` mode: `NT_NetRxGet()` returns a
pointer directly into the DMA ring buffer, and each packet's `pkt` pointer
points into that segment without copying. The segment is held open until
`NT_NetRxRelease()` is called after `arkime_packet_batch_flush()`. Arkime's
packet threads copy each packet before that release completes.

### Hardware timestamps

The FPGA timestamps packets at ingress (the moment the first byte arrives at
the physical port) and embeds a 64-bit Unix nanosecond value directly into
the packet descriptor. The plugin reads `NtStd0Descr_t.timestamp` and stores
it as `packet->ts`. Arkime's processing pipeline never overwrites `packet->ts`,
so all session timestamps and pcap headers reflect the hardware ingress time,
not when Arkime processed the packet. This allows the Napatech card to buffer 
packets keeping the correct timestamps until Arkime can process them with no loss 
unless the specified buffer runs out. 

Requires `TimestampFormat = UNIX_NS` in `ntservice.ini`.

---

## Boot Persistence

Load the kernel module at boot:

```sh
echo "nt3gd" | sudo tee /etc/modules-load.d/napatech.conf
```

For `ntservice`, create a systemd unit or use the one provided by the
Link-Capture Software installer.
