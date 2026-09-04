"""Pilote OBS par websocket et observe le DMX qui en sort."""
import base64, hashlib, json, socket, struct, threading, time, sys
from urllib.request import urlopen  # noqa: F401  (garde stdlib only)

PASSWORD = sys.argv[1]
PORT = 4455

# --- recepteur Art-Net, en tache de fond -----------------------------------
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

# --- client websocket minimal (obs-websocket v5) ---------------------------
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
            if not chunk: raise ConnectionError("ferme")
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
assert ws.recv()["op"] == 2, "identification refusee"
print("connecte a obs-websocket")

def request(kind, data=None):
    rid = str(time.time())
    ws.send({"op": 6, "d": {"requestType": kind, "requestId": rid, "requestData": data or {}}})
    while True:
        msg = ws.recv()
        if msg["op"] == 7 and msg["d"]["requestId"] == rid:
            return msg["d"]

scenes = request("GetSceneList")["responseData"]
print("scenes :", [s["sceneName"] for s in scenes["scenes"]])

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
    """Plusieurs releves successifs, pour observer ce qui bouge dans le temps."""
    switch(scene)
    out = []
    for _ in range(count):
        out.append(list(latest["slots"]))
        time.sleep(interval)
    return out

a = snapshot("Programme 1 : plateau chaud", "Plateau")
b = snapshot("Programme 2 : interview bleue", "Interview")
c = snapshot("Retour au plateau", "Plateau")

# --- effets ---------------------------------------------------------------
print("\n--- Chaser ---")
chase = sample("Chaser", 12, 0.12)
# Les intensites des quatre projecteurs, adresses 1, 11, 21, 31.
motifs = {tuple(t[a] for a in (0, 10, 20, 30)) for t in chase}
for m in sorted(motifs):
    print(f"  motif {m}")

print("\n--- Strobe sur fond bleu ---")
strobe = snapshot("Strobe materiel", "Strobe")

print("\n--- Effet embarque ---")
fxslots = snapshot("Orage dans la lampe", "EffetIntegre")
print(f"  T4c effets, canaux 100-108: {fxslots[99:108]}")

print("\n--- Canaux saisis a la main ---")
manuel = snapshot("Canaux forces", "FxManuel")

print("\n--- Reaction au son ---")
# Meme programme, sans source sonore : le temoin.
silence = sample("Silence", 3, 0.3)[-1]
print(f"  temoin silencieux, canaux 1-9 : {silence[0:9]}")
musique = sample("Musique", 14, 0.25)
niveaux = [t[0] for t in musique]
print(f"  intensites relevees : {niveaux}")

print(f"\ntrames Art-Net recues : {latest['count']}")
stop.set(); time.sleep(0.4)

# --- verifications ---------------------------------------------------------
ok = True
def check(label, cond):
    global ok
    print(f"  {'OK  ' if cond else 'ECHEC'} {label}")
    ok = ok and cond

print("\nverifications :")
check("le changement de scene change bien le DMX", a != b)
check("revenir a la scene rejoue le meme etat", a == c)
# Plateau : blanc 3200 K, pleine intensite, fondu couleur ferme.
check("plateau — intensite a fond", a[0] == 255)
check("plateau — fondu vers la couleur ferme", a[3] == 0)
check("plateau — saturation nulle", a[5] == 0)
check("plateau — 3200 K sur la plage 2500-7500", abs(a[1] - 36) <= 2)
# Interview : bleu sature a 60 %.
check("interview — intensite a 60 %", abs(b[0] - 153) <= 2)
check("interview — fondu vers la couleur ouvert", b[3] == 255)
check("interview — teinte bleue (240 degres)", abs(b[4] - 170) <= 2)
check("interview — saturation maximale", b[5] == 255)
# Les deux projecteurs doivent recevoir la meme chose, a 9 canaux d'ecart.
check("les deux projecteurs sont pilotes a l'identique", a[0:9] == a[10:19] and b[0:9] == b[10:19])
# Le vert/magenta neutre du T4c est a 132, pas au milieu de 0-255.
check("vert/magenta au neutre constructeur (132)", a[2] == 132 and b[2] == 132)
check("strobe eteint", a[8] == 0 and b[8] == 0)

print("\neffets :")
# Un chaser a deux pas sur quatre projecteurs : un sur deux allume, et le motif
# doit s'inverser au fil du temps.
check("le chaser alterne un projecteur sur deux", (255, 0, 255, 0) in motifs or (0, 255, 0, 255) in motifs)
check("le chaser se deplace dans le temps", len(motifs) >= 2)

# Le T4c a un canal de strobe : il doit etre pilote plutot que module par nous.
check("le strobe passe par le canal materiel", strobe[8] >= 20)
check("le strobe garde le fond allume", strobe[0] > 0)
check("le strobe garde la couleur du programme", strobe[3] == 255 and abs(strobe[4] - 170) <= 2)

# Mode FX : canal 3 du bloc, soit l'adresse 102, porte le choix de l'effet.
check("l'effet embarque selectionne l'orage", fxslots[101] == 15)
check("l'effet embarque est lance, pas arrete", fxslots[100] < 10)
check("la vitesse de l'effet est ecrite", 30 <= fxslots[104] <= 39)

check("le canal 5 saisi a la main sort a 200", manuel[4] == 200)
check("le canal 6 saisi a la main sort a 111", manuel[5] == 111)
# Le canal 99 depasse les 9 canaux du T4c : l'ecrire piloterait son voisin.
check("un canal hors de l'appareil n'ecrase pas son voisin", manuel[9] == 0 and manuel[98] == 0)

check("le meme programme sans son laisse la lumiere eteinte", silence[0] == 0)
check("avec le son, la lumiere s'allume", max(niveaux) > 60)
# La couleur propre a l'effet doit sortir, et non celle du programme.
teintes = [t[4] for t in musique if t[0] > 60]
check("l'effet impose bien sa propre couleur (magenta)",
      bool(teintes) and all(abs(h - 212) <= 3 for h in teintes))

print("\nRESULTAT :", "tout est conforme" if ok else "AU MOINS UNE VERIFICATION A ECHOUE")
sys.exit(0 if ok else 1)
