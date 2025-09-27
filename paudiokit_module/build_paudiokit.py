# build_paudiokit.py
from cffi import FFI

ffibuilder = FFI()

# 1) Déclarations UNIQUEMENT (pas de directives #, pas de corps de fonction)
ffibuilder.cdef("""
    int retrieve_wav_data(char *filename, struct HEADER* out);
    
    struct HEADER {
        unsigned char riff[4];
        unsigned int overall_size;
        unsigned char wave[4];
        unsigned char fmt_chunk_marker[4];
        unsigned int length_of_fmt;
        unsigned int format_type;
        unsigned int channels;
        unsigned int sample_rate;
        unsigned int byterate;
        unsigned int block_align;
        unsigned int bits_per_sample;
        unsigned char data_chunk_header[4];
        unsigned int data_size;
    };
""")

# 2) Compilation de tes SOURCES .c (pas d'archive .a)
ffibuilder.set_source(
    "_audiokit",                     # nom du module Python généré
    '#include "audiokit_cffi.h"',    # petite “glue” C : inclut ton header public allégé
    sources=["../src/audiokit.c"],      # <-- on compile directement tes .c en PIC
    include_dirs=["../src/"],            # où trouver audiokit_cffi.h (et tes .h)
    # libraries=["sndfile", "fftw3"],# si tu dépends de libs externes
    # library_dirs=[...],            # si besoin de dossiers spéciaux pour ces libs externes
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)

