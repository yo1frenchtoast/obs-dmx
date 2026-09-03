"""Genere un son de test : une basse continue a 60 Hz, dans le grave."""
import math, struct, sys, wave

path = sys.argv[1]
rate, seconds, freq = 48000, 4.0, 60.0
with wave.open(path, "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate)
    frames = bytearray()
    for n in range(int(rate * seconds)):
        v = int(0.8 * 32767 * math.sin(2 * math.pi * freq * n / rate))
        frames += struct.pack("<hh", v, v)
    w.writeframes(bytes(frames))
print(f"{path} : {seconds}s a {freq} Hz")
