import socket, struct, sys, time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 6454))
s.settimeout(float(sys.argv[1]) if len(sys.argv) > 1 else 3.0)

frames, t0, tlast, seqs = 0, None, None, []
try:
    while True:
        data, _ = s.recvfrom(2048)
        if not data.startswith(b"Art-Net\0"):
            continue
        op, = struct.unpack_from("<H", data, 8)
        if op != 0x5000:
            continue
        tlast = time.monotonic()
        if t0 is None:
            t0 = tlast
        ver = struct.unpack_from(">H", data, 10)[0]
        seq, phys, subuni, net = data[12], data[13], data[14], data[15]
        length = struct.unpack_from(">H", data, 16)[0]
        slots = data[18:18+length]
        frames += 1
        seqs.append(seq)
        if frames == 1:
            print(f"first frame : version={ver} net={net} subuni={subuni} longueur={length}")
            print(f"           canaux 1-6 = {list(slots[:6])}  canal 512 = {slots[511]}")
except socket.timeout:
    pass

if frames:
    dur = tlast - t0
    print(f"\n{frames} frames in {dur:.2f}s  ->  {frames/dur:.1f} Hz")
    # Art-Net: the sequence runs 0x01 to 0xff, 0x00 meaning "disabled".
    # Going from 255 to 1 is therefore a legitimate step, not a jump.
    def suivant(a, b):
        return b == (a + 1) if a < 255 else b == 1
    fautes = [(a, b) for a, b in zip(seqs, seqs[1:]) if not suivant(a, b)]
    print(f"sequence conformant (0x01-0xff, no 0) : {not fautes}")
    if fautes:
        print(f"  {len(fautes)} rupture(s), p.ex. {fautes[:5]}")
    print(f"no sequence at 0 (reserved value) : {0 not in seqs}")
else:
    print("no ArtDMX frame received")
