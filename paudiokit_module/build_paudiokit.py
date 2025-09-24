# build_paudiokit.py
from cffi import FFI

ffibuilder = FFI()

# 1) Déclarations UNIQUEMENT (pas de directives #, pas de corps de fonction)
ffibuilder.cdef("""
    // exemples — adapte à TON API réelle :
    int retrieve_wav_data(const char *filename);
    // ajoute ici d'autres prototypes/typedef/struct/enum si nécessaires
""")

# 2) Compilation de tes SOURCES .c (pas d'archive .a)
ffibuilder.set_source(
    "_audiokit",                     # nom du module Python généré
    '#include "audiokit_cffi.h"',    # petite “glue” C : inclut ton header public allégé
    sources=["lib/audiokit.c"],      # <-- on compile directement tes .c en PIC
    include_dirs=["lib"],            # où trouver audiokit_cffi.h (et tes .h)
    # libraries=["sndfile", "fftw3"],# si tu dépends de libs externes
    # library_dirs=[...],            # si besoin de dossiers spéciaux pour ces libs externes
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)

