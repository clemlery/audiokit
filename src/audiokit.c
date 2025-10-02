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

/**
 * Prints the data from the buffer data specified
 * @param wh a struct representing the WAV header
 * @param buffer Pointer to unsigned char* that will point to audio data
 */
void print_data(struct wav_header *wh, unsigned char *buffer, int frames_to_print)
{
    const int channels = wh->num_channels;
    const int bytes_per_sample = wh->bits_per_sample / 8;     // supposé 2 ici
    const int bytes_per_frame  = wh->block_align;             // = channels * bytes_per_sample

    if (wh->audio_format != 1 || wh->bits_per_sample != 16) {
        fprintf(stderr, "Cette fonction suppose PCM 16-bit.\n");
        return;
    }

    int max_frames = (int)(wh->subchunk2_size / bytes_per_frame);
    if (frames_to_print > max_frames) frames_to_print = max_frames;

    for (int f = 0; f < frames_to_print; ++f) {
        size_t base = (size_t)f * (size_t)bytes_per_frame;
        printf("%d.", f);

        for (int ch = 0; ch < channels; ++ch) {
            size_t off = base + (size_t)ch * (size_t)bytes_per_sample;

            // Little-endian: low, high
            uint16_t u = (uint16_t)buffer[off]
                       | ((uint16_t)buffer[off + 1] << 8);
            int16_t  s = (int16_t)u;  // interprétation signée (audio classique)

            // Affiche signé (valeur audio) et non signé si tu veux comparer
            printf(" ch%d:%d(u=%u hex=0x%04X)", ch, (int)s, (unsigned)u, (unsigned)u);
        }
        printf("\n");
    }
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
            printf("Premiers octets lus format hexadecimal :\n");
            int samples_index = 0;
            for (int i = 0; i < 100 && i < wav_hdr.subchunk2_size; i++)
            {
                if (i % wav_hdr.block_align == 0)
                {
                    printf("\n");
                    printf("%d. ", samples_index);
                    samples_index++;
                }
                printf("%02X ", audio_buffer[i]);
            }
            printf("\n");

            printf("Premiers octets lus format entier :\n");
            print_data(&wav_hdr, audio_buffer, 50);

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