#ifndef WAV_READER_WAV_READER_TYPES_H
#define WAV_READER_WAV_READER_TYPES_H

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

#endif //WAV_READER_WAV_READER_TYPES_H