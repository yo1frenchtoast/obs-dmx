"""Drives OBS over websocket and watches the DMX that comes out."""
import base64, hashlib, json, socket, struct, threading, time, sys
from urllib.request import urlopen  # noqa: F401  (garde stdlib only)

PASSWORD = sys.argv[1]
PORT = 4455

# --- Art-Net receiver, in the background -----------------------------------
latest = {"slots": None, "count": 0}
stop = threading.Event()

def listen():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 6454)); s.settimeout(0.3)
    while not stop.is_set():
        try:
            data, _ = s.recvfrom(2048)
        except socket.timeout:
            continue
        if data.startswith(b"Art-Net\0") and struct.unpack_from("<H", data, 8)[0] == 0x5000:
            latest["slots"] = list(data[18:18 + struct.unpack_from(">H", data, 16)[0]])
            latest["count"] += 1
    s.close()

threading.Thread(target=listen, daemon=True).start()

# --- minimal websocket client (obs-websocket v5) ---------------------------
class WS:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=10)
        key = base64.b64encode(b"0123456789abcdef").decode()
        self.sock.sendall(
            f"GET /websocket HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
            f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
            .encode())
        buf = b""
        while b"\r\n\r\n" not in buf:
            buf += self.sock.recv(4096)
        assert b"101" in buf.split(b"\r\n")[0], buf[:120]
        self.buf = buf.split(b"\r\n\r\n", 1)[1]

    def send(self, obj):
        payload = json.dumps(obj).encode()
        header = bytearray([0x81])
        n = len(payload)
        if n < 126: header.append(0x80 | n)
        elif n < 65536: header.append(0x80 | 126); header += struct.pack(">H", n)
        else: header.append(0x80 | 127); header += struct.pack(">Q", n)
        mask = b"\x00\x00\x00\x00"
        self.sock.sendall(bytes(header) + mask + payload)

    def _read(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk: raise ConnectionError("closed")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def recv(self):
        b1, b2 = self._read(2)
        n = b2 & 0x7F
        if n == 126: n = struct.unpack(">H", self._read(2))[0]
        elif n == 127: n = struct.unpack(">Q", self._read(8))[0]
        return json.loads(self._read(n))

ws = WS("127.0.0.1", PORT)
hello = ws.recv()
auth = hello["d"].get("authentication")
identify = {"op": 1, "d": {"rpcVersion": 1}}
if auth:
    secret = base64.b64encode(hashlib.sha256((PASSWORD + auth["salt"]).encode()).digest()).decode()
    identify["d"]["authentication"] = base64.b64encode(
        hashlib.sha256((secret + auth["challenge"]).encode()).digest()).decode()
ws.send(identify)
assert ws.recv()["op"] == 2, "identification refused"
print("connected to obs-websocket")

def request(kind, data=None):
    rid = str(time.time())
    ws.send({"op": 6, "d": {"requestType": kind, "requestId": rid, "requestData": data or {}}})
    while True:
        msg = ws.recv()
        if msg["op"] == 7 and msg["d"]["requestId"] == rid:
            return msg["d"]

scenes = request("GetSceneList")["responseData"]
print("scenes:", [s["sceneName"] for s in scenes["scenes"]])

def switch(scene, settle=1.2):
    request("SetCurrentProgramScene", {"sceneName": scene})
    time.sleep(settle)

def snapshot(label, scene):
    switch(scene)
    slots = latest["slots"]
    print(f"\n--- {label} (scene '{scene}') ---")
    print(f"  T4c 1, canaux  1-9 : {slots[0:9]}")
    print(f"  T4c 2, canaux 11-19: {slots[10:19]}")
    return slots

def sample(scene, count, interval):
    """Several successive readings, to watch what moves over time."""
    switch(scene)
    out = []
    for _ in range(count):
        out.append(list(latest["slots"]))
        time.sleep(interval)
    return out

a = snapshot("Programme 1: warm stage look", "Plateau")
b = snapshot("Programme 2: blue interview", "Interview")
c = snapshot("Back to the stage look", "Plateau")

# --- effects ---------------------------------------------------------------
print("\n--- Chase ---")
chase = sample("Chase", 12, 0.12)
# Intensities of the four fixtures, at addresses 1, 11, 21, 31.
patterns = {tuple(t[a] for a in (0, 10, 20, 30)) for t in chase}
for m in sorted(patterns):
    print(f"  pattern {m}")

print("\n--- Strobe sur fond bleu ---")
strobe = snapshot("Hardware strobe", "Strobe")

print("\n--- Built-in effect ---")
fxslots = snapshot("Lightning inside the fixture", "EffetIntegre")
print(f"  T4c effets, canaux 100-108: {fxslots[99:108]}")

print("\n--- Hand-entered channels ---")
manuel = snapshot("Forced channels", "FxManuel")

print("\n--- Sound reaction ---")
# Same programme, no sound source: the control.
silence = sample("Silence", 3, 0.3)[-1]
print(f"  silent control, channels 1-9 : {silence[0:9]}")
musique = sample("Musique", 14, 0.25)
niveaux = [t[0] for t in musique]
print(f"  intensities read : {niveaux}")

print(f"\nArt-Net frames received : {latest['count']}")
stop.set(); time.sleep(0.4)

# --- checks ----------------------------------------------------------------
ok = True
def check(label, cond):
    global ok
    print(f"  {'OK  ' if cond else 'FAIL'} {label}")
    ok = ok and cond

print("\nchecks:")
check("switching scene really does change the DMX", a != b)
check("returning to a scene replays the same state", a == c)
# Stage look: 3200 K white, full intensity, colour crossfade closed.
check("stage look - intensity at full", a[0] == 255)
check("stage look - colour crossfade closed", a[3] == 0)
check("stage look - zero saturation", a[5] == 0)
check("stage look - 3200 K over the 2500-7500 range", abs(a[1] - 36) <= 2)
# Interview: saturated blue at 60%.
check("interview - intensity at 60%", abs(b[0] - 153) <= 2)
check("interview - colour crossfade open", b[3] == 255)
check("interview - blue hue (240 degrees)", abs(b[4] - 170) <= 2)
check("interview - full saturation", b[5] == 255)
# Both fixtures must receive the same thing, nine channels apart.
check("both fixtures are driven identically", a[0:9] == a[10:19] and b[0:9] == b[10:19])
# The T4c's green/magenta neutral is at 132, not the middle of 0-255.
check("green/magenta at the manufacturer's neutral (132)", a[2] == 132 and b[2] == 132)
check("strobe off", a[8] == 0 and b[8] == 0)

print("\neffects:")
# A two-step chase over four fixtures: every other one lit, and the pattern must
# invert over time.
check("the chase alternates every other fixture", (255, 0, 255, 0) in patterns or (0, 255, 0, 255) in patterns)
check("the chase moves over time", len(patterns) >= 2)

# The T4c has a strobe channel: it should be driven rather than modulated by us.
check("the strobe goes through the hardware channel", strobe[8] >= 20)
check("the strobe keeps the background lit", strobe[0] > 0)
check("the strobe keeps the programme's colour", strobe[3] == 255 and abs(strobe[4] - 170) <= 2)

# FX mode: channel 3 of the block, address 102, carries the effect selection.
check("the built-in effect selects lightning", fxslots[101] == 15)
check("the built-in effect is running, not stopped", fxslots[100] < 10)
check("the effect's rate is written", 30 <= fxslots[104] <= 39)

check("hand-entered channel 5 comes out at 200", manuel[4] == 200)
check("hand-entered channel 6 comes out at 111", manuel[5] == 111)
# Channel 99 is past the T4c's 9 channels: writing it would drive its neighbour.
check("a channel past the fixture does not overwrite its neighbour", manuel[9] == 0 and manuel[98] == 0)

check("the same programme without sound leaves the lights out", silence[0] == 0)
check("with sound, the lights come up", max(niveaux) > 60)
# The effect's own colour must come out, not the programme's.
teintes = [t[4] for t in musique if t[0] > 60]
check("the effect does impose its own colour (magenta)",
      bool(teintes) and all(abs(h - 212) <= 3 for h in teintes))

print("\nRESULT:", "everything checks out" if ok else "AT LEAST ONE CHECK FAILED")
sys.exit(0 if ok else 1)
