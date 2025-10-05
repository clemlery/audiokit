import os
import time
import gc
import numpy as np
import librosa
from paudiokit_module.audiokit.audiokit import Audiokit  # <-- si ta classe est importée ainsi
from typing import Final

FILENAME : Final[str] = "./data/file_example_WAV_2MG.wav"

# ------------------------------------------------------------
# 🔧 Configuration du benchmark
# ------------------------------------------------------------
FRAME_LENGTH = 2048
HOP_LENGTH = 512
CENTER = 0  # 0 = False pour audiokit

# ------------------------------------------------------------
# 🚀 Chargement des données
# ------------------------------------------------------------
print(f"Chargement du fichier audio : {FILENAME}")

# Chargement avec ton module C/Python
audiokit = Audiokit(FILENAME)

# Chargement avec librosa (sans conversion mono)
y, sr = librosa.load(FILENAME, sr=None, mono=False)
print(f"librosa → shape: {y.shape}, sr={sr}")

# Sélection du premier canal pour la comparaison
y_ch1 = y[0] if y.ndim == 2 else y

# ------------------------------------------------------------
# ⚙️ Définition des fonctions à benchmarker
# ------------------------------------------------------------
def run_audiokit():
    return audiokit.zero_crossing_rate(FRAME_LENGTH, HOP_LENGTH, CENTER)[0]

def run_librosa():
    return librosa.feature.zero_crossing_rate(
        y_ch1,
        frame_length=FRAME_LENGTH,
        hop_length=HOP_LENGTH,
        center=False
    )

# ------------------------------------------------------------
# 🔥 Fonction de mesure robuste
# ------------------------------------------------------------
def bench(fn, repeats=10):
    gc_old = gc.isenabled()
    gc.disable()
    try:
        t0 = time.perf_counter()
        res = None
        for _ in range(repeats):
            res = fn()
        t1 = time.perf_counter()
        elapsed = (t1 - t0) / repeats
    finally:
        if gc_old:
            gc.enable()
    return elapsed, res

# ------------------------------------------------------------
# 🧠 Warm-up (important pour éviter les artefacts)
# ------------------------------------------------------------
for _ in range(3):
    _ = run_audiokit()
    _ = run_librosa()

# ------------------------------------------------------------
# ⏱️ Benchmark
# ------------------------------------------------------------
N = 20
t_audiokit, zcr_audiokit = bench(run_audiokit, repeats=N)
t_librosa, zcr_librosa = bench(run_librosa, repeats=N)

# ------------------------------------------------------------
# 📊 Résultats
# ------------------------------------------------------------
print("\n===== Résultats du benchmark =====")
print(f"Temps moyen audiokit : {t_audiokit*1e3:.3f} ms/appel")
print(f"Temps moyen librosa  : {t_librosa*1e3:.3f} ms/appel")

# ------------------------------------------------------------
# 📈 Quelques statistiques
# ------------------------------------------------------------
def stats(name, z):
    z = np.asarray(z)
    print(f"{name}: shape={z.shape}, mean={z.mean():.6f}, std={z.std():.6f}, min={z.min():.6f}, max={z.max():.6f}")

stats("audiokit", zcr_audiokit)
stats("librosa ", zcr_librosa)

# ------------------------------------------------------------
# 🧮 Comparaison des valeurs (si les tailles correspondent)
# ------------------------------------------------------------
za = np.asarray(zcr_audiokit).squeeze()
zl = np.asarray(zcr_librosa).squeeze()

if za.shape == zl.shape:
    mae = np.mean(np.abs(za - zl))
    print(f"Erreur moyenne absolue audiokit vs librosa : {mae:.6e}")
else:
    print(f"⚠️ Dimensions différentes : audiokit {za.shape}, librosa {zl.shape}")
