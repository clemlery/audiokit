// WAVE file header format
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
ErrorCode zero_crossing_rate(int16_t *samples, int frame_number, float *zcr_output)

// This function is used to retrive data in Wave file specified by its path in function parameters
int retrieve_wav_data(char *filename, struct wav_header *out_wh, int16_t **out_samples, uint32_t *out_frames);

