# build_paudiokit.py
from cffi import FFI

ffibuilder = FFI()

# 1) Déclarations UNIQUEMENT (pas de directives #, pas de corps de fonction)
ffibuilder.cdef("""
    struct wav_header {
        // RIFF chunk descriptor
        char     chunk_id[5];    // "RIFF"
        uint32_t chunk_size;     // taille fichier - 8
        char     format[5];      // "WAVE"

        // fmt subchunk
        char     subchunk1_id[5]; // "fmt "
        uint32_t subchunk1_size;  // 16 pour PCM
        uint16_t audio_format;    // 1 = PCM, 3 = IEEE float
        uint16_t num_channels;    // 1 = mono, 2 = stéréo...
        uint32_t sample_rate;     // ex: 44100
        uint32_t byte_rate;       // sample_rate * num_channels * bits_per_sample/8
        uint16_t block_align;     // num_channels * bits_per_sample/8
        uint16_t bits_per_sample; // 8, 16, 24, 32

        // data subchunk
        char     subchunk2_id[5]; // "data"
        uint32_t subchunk2_size;  // nombre d’octets de données
    };
    
    typedef enum {
        ERR_OK = 0,
        ERR_INVALID_ARG,
        ERR_IO,
        ERR_FORMAT,
        ERR_OUT_OF_MEMORY,
        ERR_INTERNAL
    } ErrorCode;
    
    int retrieve_wav_data(char *filename, struct wav_header *out_wh, int16_t **out_samples, uint32_t *out_frames);
    
    ErrorCode zero_crossing_rate(int16_t *samples, int frame_number, float *zcr_output);
    
    ErrorCode last_error_code(void);

    const char *last_error_message(void);
""")

# 2) Compilation de tes SOURCES .c (pas d'archive .a)
ffibuilder.set_source(
    "_audiokit",                     # nom du module Python généré
    '#include "audiokit_cffi.h"',    # petite “glue” C : inclut header public allégé
    sources=["../src/audiokit.c"],      # <-- on compile directement tes .c en PIC
    include_dirs=["../src/"],            
    # library_dirs=[...],            # si besoin de dossiers spéciaux pour ces libs externes
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)

