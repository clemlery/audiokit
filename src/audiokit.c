/**
 * Read and parse a wave file
 *
 **/
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "audiokit.h"
#include "audiokit_types.h"

#define TRUE 1
#define FALSE 0

// WAVE header structure

unsigned char buffer4[4];
unsigned char buffer2[2];

FILE *ptr;

static _Thread_local ErrorContext last_error = {ERR_OK, NULL};

// ########################################## ERROR HANDLERS ##########################################

static void set_error(ErrorCode code, const char *msg)
{
    last_error.code = code;
    last_error.msg = msg;
}

ErrorCode last_error_code(void)
{
    return last_error.code;
}

const char *last_error_message(void)
{
    return last_error.msg ? last_error.msg : "";
}

/**
 * Reads the complete WAV header (RIFF + fmt + data)
 * @param fp A pointer to a WAV file
 * @return a struct representing the WAV header
 */
struct wav_header read_wav_header(FILE *fp)
{
    struct wav_header hdr;
    memset(&hdr, 0, sizeof(hdr));

    // RIFF
    fread(hdr.chunk_id, 4, 1, fp);
    hdr.chunk_id[4] = '\0';
    fread(&hdr.chunk_size, 4, 1, fp);
    fread(hdr.format, 4, 1, fp);
    hdr.format[4] = '\0';

    // fmt
    fread(hdr.subchunk1_id, 4, 1, fp);
    hdr.subchunk1_id[4] = '\0';
    fread(&hdr.subchunk1_size, 4, 1, fp);
    fread(&hdr.audio_format, 2, 1, fp);
    fread(&hdr.num_channels, 2, 1, fp);
    fread(&hdr.sample_rate, 4, 1, fp);
    fread(&hdr.byte_rate, 4, 1, fp);
    fread(&hdr.block_align, 2, 1, fp);
    fread(&hdr.bits_per_sample, 2, 1, fp);

    if (hdr.subchunk1_size > 16)
    {
        uint16_t extra_param_size;
        fread(&extra_param_size, 2, 1, fp);
        fseek(fp, extra_param_size, SEEK_CUR);
    }

    // data
    fread(hdr.subchunk2_id, 4, 1, fp);
    hdr.subchunk2_id[4] = '\0';
    fread(&hdr.subchunk2_size, 4, 1, fp);

    return hdr;
}

/**
 * Reads the data subchunk of the WAV file and stores audio samples in a buffer.
 * The buffer is allocated dynamically and must be freed by the caller.
 *
 * @param fp   A pointer to an open WAV file (already positioned at "data")
 * @param ds   Pointer to a struct to store metadata (ID + size)
 * @param buffer Pointer to unsigned char* that will point to audio data
 * @return 0 on success, -1 on error
 */
int read_data_subchunk(FILE *fp, struct wav_header *hdr, unsigned char **buffer)
{
    if (!fp || !hdr || !buffer)
        return -1;

    // Allocation du buffer
    *buffer = (unsigned char *)malloc(hdr->subchunk2_size);
    if (!*buffer)
        return -1;

    // Lecture des données audio
    size_t read_bytes = fread(*buffer, 1, hdr->subchunk2_size, fp);
    if (read_bytes != hdr->subchunk2_size)
    {
        free(*buffer);
        *buffer = NULL;
        return -1;
    }

    return 0;
}

ErrorCode read_data(FILE *fp, int channels, int num_samples, int bits_per_sample)
{
    long i = 0;
    long data_index = 0;
    long size_of_each_sample = (channels * bits_per_sample) / 8;
    char data_buffer[size_of_each_sample];
    int size_is_correct = TRUE;
    char *error_message;

    // make sure that the bytes-per-sample is completely divisible by num.of channels
    long bytes_in_each_channel = (size_of_each_sample / channels);
    if ((bytes_in_each_channel * channels) != size_of_each_sample)
    {
        sprintf(error_message, "Error wrong file format: %ld x %u <> %ld", bytes_in_each_channel, channels, size_of_each_sample);
        set_error(ERR_FORMAT, error_message);
        size_is_correct = FALSE;
        return ERR_FORMAT;
    }

    if (size_is_correct)
    {
        // the valid amplitude range for values based on the bits per sample
        long low_limit = 0l;
        long high_limit = 0l;

        switch (bits_per_sample)
        {
        case 8:
            low_limit = -128;
            high_limit = 127;
            break;
        case 16:
            low_limit = -32768;
            high_limit = 32767;
            break;
        case 32:
            low_limit = -2147483648;
            high_limit = 2147483647;
            break;
        }

        for (i = 1; i <= num_samples; i++)
        {
            int read = fread(data_buffer, sizeof(data_buffer), 1, ptr);
            if (read == 1)
            {

                // dump the data read
                unsigned int xchannels = 0;
                int data_in_channel = 0;
                int offset = 0; // move the offset for every iteration in the loop below
                for (xchannels = 0; xchannels < channels; xchannels++)
                {
                    // convert data from little endian to big endian based on bytes in each channel sample
                    if (bytes_in_each_channel == 4)
                    {
                        data_in_channel = (data_buffer[offset] & 0x00ff) |
                                          ((data_buffer[offset + 1] & 0x00ff) << 8) |
                                          ((data_buffer[offset + 2] & 0x00ff) << 16) |
                                          (data_buffer[offset + 3] << 24);
                    }
                    else if (bytes_in_each_channel == 2)
                    {
                        data_in_channel = (data_buffer[offset] & 0x00ff) |
                                          (data_buffer[offset + 1] << 8);
                    }
                    else if (bytes_in_each_channel == 1)
                    {
                        data_in_channel = data_buffer[offset] & 0x00ff;
                        data_in_channel -= 128; // in wave, 8-bit are unsigned, so shifting to signed
                    }

                    offset += bytes_in_each_channel;

                    // check if value was in range
                    if (data_in_channel < low_limit || data_in_channel > high_limit)
                    {
                        set_error(ERR_INTERNAL, "Value out of range");
                        return ERR_INTERNAL;
                    }

                    data_index++;
                }
            }
            else
            {
                sprintf(error_message, "Error reading file. %d bytes", read);
                set_error(ERR_INTERNAL, error_message);
                return ERR_INTERNAL;
            }

        } // 	for (i =1; i <= num_samples; i++) {

    } // 	if (size_is_correct) {
}

/**
 * Prints the read header from the WAV file
 * @param wh a struct representing the WAV header
 */
void print_wav_header(struct wav_header wh)
{
    printf("ChunkID\t\t\t%s\n", wh.chunk_id);
    printf("ChunkSize\t\t%" PRIu32 "\n", wh.chunk_size);
    printf("Format\t\t\t%s\n\n", wh.format);

    printf("Subchunk1ID\t\t%s\n", wh.subchunk1_id);
    printf("Subchunk1Size\t%" PRIu32 "\n", wh.subchunk1_size);
    printf("AudioFormat\t\t%" PRIu16 "\n", wh.audio_format);
    printf("NumChannels\t\t%" PRIu16 "\n", wh.num_channels);
    printf("SampleRate\t\t%" PRIu32 "\n", wh.sample_rate);
    printf("ByteRate\t\t%" PRIu32 "\n", wh.byte_rate);
    printf("BlockAlign\t\t%" PRIu16 "\n", wh.block_align);
    printf("BitsPerSample\t%" PRIu16 "\n\n", wh.bits_per_sample);

    printf("Subchunk2ID\t\t%s\n", wh.subchunk2_id);
    printf("Subchunk2Size\t%" PRIu32 "\n", wh.subchunk2_size);
}

int amplitude_envelope(char *filename)
{
    return 0;
}

int rms(char *filename)
{
    return 0;
}

int zcr(char *filename)
{
    return 0;
}

// ########################################## ACCESSING META-DATA ##########################################

// ########################################## HELPERS ##########################################

/**
 * Convert seconds into hh:mm:ss format
 * Params:
 *	seconds - seconds value
 * Returns: hms - formatted string
 **/
char *seconds_to_time(float raw_seconds)
{
    char *hms;
    int hours, hours_residue, minutes, seconds, milliseconds;
    hms = (char *)malloc(100);

    sprintf(hms, "%f", raw_seconds);

    hours = (int)raw_seconds / 3600;
    hours_residue = (int)raw_seconds % 3600;
    minutes = hours_residue / 60;
    seconds = hours_residue % 60;
    milliseconds = 0;

    // get the decimal part of raw_seconds to get milliseconds
    char *pos;
    pos = strchr(hms, '.');
    int ipos = (int)(pos - hms);
    char decimalpart[15];
    memset(decimalpart, ' ', sizeof(decimalpart));
    strncpy(decimalpart, &hms[ipos + 1], 3);
    milliseconds = atoi(decimalpart);

    sprintf(hms, "%d:%d:%d.%d", hours, minutes, seconds, milliseconds);
    return hms;
}

int main(int argc, char **argv)
{
    FILE *fp;

    if (argc == 2)
    {
        fp = fopen(argv[1], "rb");
        struct wav_header wav_hdr = read_wav_header(fp);
        print_wav_header(wav_hdr);
        unsigned char *audio_buffer = NULL;

        if (read_data_subchunk(fp, &wav_hdr, &audio_buffer) == 0)
        {

            // Afficher les 10 premiers octets pour vérification
            printf("Premiers octets lus :\n");
            int samples_index = 0;
            for (int i = 0; i < 100 && i < wav_hdr.subchunk2_size; i++)
            {
                if (i % wav_hdr.bits_per_sample == 0)
                {
                    printf("\n");
                    printf("%d. ", samples_index);
                    samples_index++;
                }
                printf("%02X ", audio_buffer[i]);
            }
            printf("\n");

            // Important : libérer la mémoire
            free(audio_buffer);
        }
        else
        {
            fprintf(stderr, "Erreur lecture data subchunk\n");
        }
        fclose(fp);
    }
    else
    {
        printf("You must specify only one argument that must be the relative path to a WAV file.");
    }
    return 0;
}