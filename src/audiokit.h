#ifndef AUDIOKIT_H
#define AUDIOKIT_H


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


// ########################################## ERROR HANDLERS ##########################################

// Enum used to define different type of error to return if anything goes wrong.
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_ARG,
    ERR_IO,
    ERR_FORMAT,
    ERR_OUT_OF_MEMORY,
    ERR_INTERNAL
} ErrorCode;

typedef struct {
    ErrorCode code;
    const char *msg;
} ErrorContext;

static void set_error(ErrorCode code, const char *msg);

ErrorCode last_error_code(void);

const char *last_error_message(void);


// This function is used to calculate the amplidute envelope of a loaded wav file
int amplitude_envelope(char * filename);

// This function is used to calculate the RMS (root-mean-square energy) of a loaded wav file
int rms(char * filename);

// This function is used to calculate the ZCR (zero-crossing rate) of a loaded wav file
ErrorCode zero_crossing_rate(
    const int16_t *samples,        // signal d'entrée
    size_t N,                      // nombre d'échantillons
    size_t frame_length,           // taille de fenêtre (ex: 2048)
    size_t hop_length,             // pas entre fenêtres (ex: 512)
    int center,                    // 1 = pad “centré” façon librosa, 0 = pas de pad
    float *zcr_out,                // buffer de sortie (taille >= n_frames)
    size_t *n_frames_out           // nombre de trames remplies
);

// ########################################## HELPERS ##########################################

static char *seconds_to_time(float seconds);

static inline int sgn_i16(int16_t x);

// ########################################## NEW METHODS ##########################################


int check_file_format(FILE* fp);

struct wav_header read_wav_header(FILE *fp);

int read_and_convert_data_s16le(FILE *fp, const struct wav_header *hdr, int16_t **out_samples, uint32_t *out_frames);

int retrieve_wav_data(char *filename, struct wav_header *out_wh, int16_t **out_samples, uint32_t *out_frames);

void print_wav_header(struct wav_header wh);

void print_data(struct wav_header *wh, unsigned char *buffer, int samples_to_print);

#endif // AUDIOKIT_H
