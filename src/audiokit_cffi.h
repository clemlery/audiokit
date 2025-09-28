// WAVE file header format
struct HEADER {
	unsigned char riff[4];						// RIFF string
	unsigned int overall_size	;				// overall size of file in bytes
	unsigned char wave[4];						// WAVE string
	unsigned char fmt_chunk_marker[4];			// fmt string with trailing null char
	unsigned int length_of_fmt;					// length of the format data
	unsigned int format_type;					// format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
	unsigned int channels;						// no.of channels
	unsigned int sample_rate;					// sampling rate (blocks per second)
	unsigned int byterate;						// SampleRate * NumChannels * BitsPerSample/8
	unsigned int block_align;					// NumChannels * BitsPerSample/8
	unsigned int bits_per_sample;				// bits per sample, 8- 8bits, 16- 16 bits etc
	unsigned char data_chunk_header [4];		// DATA string or FLLR string
	unsigned int data_size;						// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
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

// This function is used to retrive data in Wave file specified by its path in function parameters
ErrorCode retrieve_wav_data(char * filename, struct HEADER* header_output, int *data_output);

// This function is used to calculate the amplidute envelope of a loaded wav file
int amplitude_envelope(char * filename);

// This function is used to calculate the RMS (root-mean-square energy) of a loaded wav file
int rms(char * filename);

// This function is used to calculate the ZCR (zero-crossing rate) of a loaded wav file
int zcr(char * filename);
