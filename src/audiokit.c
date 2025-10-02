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

int read_and_convert_data_s16le(FILE *fp,
                                const struct wav_header *hdr,
                                int16_t **out_samples,
                                uint32_t *out_frames)
{
    if (!fp || !hdr || !out_samples || !out_frames)
        return -1;

    // Vérifs de format: PCM 16-bit little-endian
    if (hdr->audio_format != 1)
        return -2; // non-PCM
    if (hdr->bits_per_sample != 16)
        return -3; // non 16-bit
    if (hdr->block_align != hdr->num_channels * 2)
        return -4;

    const uint16_t channels = hdr->num_channels;
    const uint16_t bytes_per_frame = hdr->block_align;
    const uint32_t data_size = hdr->subchunk2_size;

    if (data_size == 0)
        return -5;
    if ((data_size % bytes_per_frame) != 0)
        return -6; // taille pas multiple d'une frame

    const uint32_t frames = data_size / bytes_per_frame;
    const size_t total_samples = (size_t)frames * (size_t)channels;

    int16_t *dst = (int16_t *)malloc(total_samples * sizeof(int16_t));
    if (!dst)
        return -7;

    // Lecture par blocs pour éviter un énorme buffer temporaire
    // Taille cible ~64 KiB, ajustée pour tomber sur un nombre entier de frames
    size_t target_bytes = 64 * 1024;
    size_t frames_per_chunk = target_bytes / bytes_per_frame;
    if (frames_per_chunk == 0)
        frames_per_chunk = 1;
    size_t chunk_bytes = frames_per_chunk * (size_t)bytes_per_frame;

    unsigned char *chunk = (unsigned char *)malloc(chunk_bytes);
    if (!chunk)
    {
        free(dst);
        return -8;
    }

    uint32_t frames_done = 0;
    size_t out_idx = 0;

    while (frames_done < frames)
    {
        uint32_t remaining = frames - frames_done;
        size_t this_frames = remaining < frames_per_chunk ? remaining : frames_per_chunk;
        size_t this_bytes = this_frames * (size_t)bytes_per_frame;

        size_t got = fread(chunk, 1, this_bytes, fp);
        if (got != this_bytes)
        {
            free(chunk);
            free(dst);
            return -9; // lecture incomplète/erreur
        }

        // Convertir ce bloc
        for (size_t f = 0; f < this_frames; ++f)
        {
            size_t base = f * (size_t)bytes_per_frame;
            for (uint16_t ch = 0; ch < channels; ++ch)
            {
                size_t off = base + (size_t)ch * 2; // 2 octets par sample
                // Little-endian: low, high
                uint16_t u = (uint16_t)chunk[off] | ((uint16_t)chunk[off + 1] << 8);
                dst[out_idx++] = (int16_t)u;
            }
        }

        frames_done += (uint32_t)this_frames;
    }

    free(chunk);

    // Sorties
    *out_samples = dst;
    *out_frames = frames;
    return 0;
}

int retrieve_wav_data(char *filename, struct wav_header *out_wh, int16_t **out_samples, uint32_t *out_frames)
{
    // We initiate the file pointer 
    FILE *fp;
    // We open the file
    fp = fopen(filename, "rb");
    // We read the header of the wav file we opened
    *out_wh = read_wav_header(fp);
    // We initiate the pointers that allow us to store the wav file data


    int error_code = read_and_convert_data_s16le(fp, out_wh, out_samples, out_frames);
    return error_code;
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
 * This code only works for a PCM format equal to 1, and a bits per sample value equal to 16
 * @param wh a struct representing the WAV header
 * @param buffer Pointer to unsigned char* that will point to audio data
 */
void print_data(struct wav_header *wh, unsigned char *buffer, int frames_to_print)
{
    const int channels = wh->num_channels;
    const int bytes_per_sample = wh->bits_per_sample / 8; // supposé 2 ici
    const int bytes_per_frame = wh->block_align;          // = channels * bytes_per_sample

    if (wh->audio_format != 1 || wh->bits_per_sample != 16)
    {
        fprintf(stderr, "Cette fonction suppose PCM 16-bit.\n");
        return;
    }

    int max_frames = (int)(wh->subchunk2_size / bytes_per_frame);
    if (frames_to_print > max_frames)
        frames_to_print = max_frames;

    for (int f = 0; f < frames_to_print; ++f)
    {
        size_t base = (size_t)f * (size_t)bytes_per_frame;
        printf("%d.", f);

        for (int ch = 0; ch < channels; ++ch)
        {
            size_t off = base + (size_t)ch * (size_t)bytes_per_sample;

            // Little-endian: low, high
            uint16_t u = (uint16_t)buffer[off] | ((uint16_t)buffer[off + 1] << 8);
            int16_t s = (int16_t)u; // interprétation signée (audio classique)

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

// int main(int argc, char **argv)
// {
//     struct wav_header wh;
//     int16_t *samples = NULL;
//     uint32_t frames = 0;
//     int error_code = retrieve_wav_data(argv[1], &wh, &samples, &frames);

//     return 0;
// }