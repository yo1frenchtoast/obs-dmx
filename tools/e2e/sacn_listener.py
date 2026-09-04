import socket, struct, sys, time

universe = int(sys.argv[1]) if len(sys.argv) > 1 else 1
timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
group = f"239.255.{(universe >> 8) & 0xFF}.{universe & 0xFF}"

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("", 5568))
# Subscribe on every interface: the sender picks its own from the default
# route, which is not necessarily the loopback.
for iface in ("0.0.0.0", "127.0.0.1"):
    try:
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                     struct.pack("4s4s", socket.inet_aton(group), socket.inet_aton(iface)))
    except OSError as e:
        print(f"  (abonnement via {iface} refuse : {e})")
s.settimeout(timeout)
print(f"listening on {group}:5568 (univers {universe})")

frames, t0, tlast, seqs = 0, None, None, []
try:
    while True:
        data, _ = s.recvfrom(2048)
        if len(data) < 126 or data[4:16] != b"ASC-E1.17\0\0\0":
            continue
        tlast = time.monotonic()
        if t0 is None:
            t0 = tlast
            root_len = struct.unpack_from(">H", data, 16)[0]
            fram_len = struct.unpack_from(">H", data, 38)[0]
            dmp_len  = struct.unpack_from(">H", data, 115)[0]
            name = data[44:108].split(b"\0")[0].decode()
            univ = struct.unpack_from(">H", data, 113)[0]
            cid  = data[22:38].hex()
            print(f"taille={len(data)}  univers={univ}  priorite={data[108]}  source='{name}'")
            print(f"PDU racine=0x{root_len:04x} tramage=0x{fram_len:04x} dmp=0x{dmp_len:04x}")
            print(f"code de depart={data[125]}  nb valeurs={struct.unpack_from('>H', data, 123)[0]}")
            print(f"CID={cid}")
            print(f"canaux 1-4 = {list(data[126:130])}")
        frames += 1
        seqs.append(data[111])
except socket.timeout:
    pass

if frames:
    print(f"\n{frames} frames in {tlast-t0:.2f}s  ->  {(frames-1)/(tlast-t0):.1f} Hz")
    ok = all(b == (a + 1) % 256 for a, b in zip(seqs, seqs[1:]))
    print(f"sequence increments (E1.31 uses the full range) : {ok}")
else:
    print("no E1.31 packet received")
