import socket
import select
import sys

# Number of PX4 instances (0..N-1) to relay between:
#   python3 packet_forwarder.py [N] [--companion-base PORT]
# With --companion-base, the mesh is also relayed to each vehicle's ROS 2 Swarm mode node
# (UDP PORT+i), so the external mode hears the same neighbours as the internal one.
args = [a for a in sys.argv[1:]]
COMPANION_BASE = None
if "--companion-base" in args:
    i = args.index("--companion-base")
    COMPANION_BASE = int(args[i + 1])
    del args[i:i + 2]
NODES = list(range(int(args[0]) if args else 3))
BASE_RX = 15200  # Python receives PX4 output
BASE_TX = 15100  # Python pushes input to PX4

def main():
    node_sockets = {}
    rx_to_node = {}

    for node_id in NODES:
        rx_port = BASE_RX + node_id
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Enable complete socket reuse
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        try:
            # Bind to 0.0.0.0 instead of 127.0.0.1 to catch all interface traffic
            sock.bind(("0.0.0.0", rx_port))
            node_sockets[node_id] = sock
            rx_to_node[sock] = node_id
            print(f"[+] Bound Node {node_id} on 0.0.0.0:{rx_port}")
        except Exception as e:
            print(f"[!] Bind failed on port {rx_port}: {e}")
            sys.exit(1)

    print(f"\n[==] RAW MESH RELAY ACTIVE FOR {len(NODES)} NODES [==]\n", flush=True)
    if COMPANION_BASE is not None:
        print(f"[==] also relaying to ROS 2 companions on UDP {COMPANION_BASE}+i [==]\n", flush=True)

    while True:
        readable, _, _ = select.select(list(rx_to_node.keys()), [], [], 0.5)
        for sock in readable:
            payload, addr = sock.recvfrom(4096)
            src_node = rx_to_node[sock]

            # Relay payload to all other nodes on loopback
            for target_id in NODES:
                if target_id != src_node:
                    target_port = BASE_TX + target_id
                    sock.sendto(payload, ("127.0.0.1", target_port))
                    if COMPANION_BASE is not None:
                        sock.sendto(payload, ("127.0.0.1", COMPANION_BASE + target_id))

if __name__ == "__main__":
    main()
