/**
 * Read and parse a wave file
 *
 **/
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "audiokit.h"

#define TRUE 1
#define FALSE 0

// WAVE header structure

unsigned char buffer4[4];
unsigned char buffer2[2];


FILE *ptr;

static _Thread_local ErrorContext last_error = { ERR_OK, NULL };


// ########################################## ERROR HANDLERS ##########################################


static void set_error(ErrorCode code, const char *msg) {
    last_error.code = code;
    last_error.msg  = msg;
}

ErrorCode last_error_code(void) {
    return last_error.code;
}

const char *last_error_message(void) {
    return last_error.msg ? last_error.msg : "";
}

ErrorCode retrieve_wav_data(char *filename, struct HEADER *out)
{

    printf("Size of struct header %ld", sizeof(struct HEADER));
    
    struct HEADER header;

    // test if file exists
    if (access(filename, F_OK) == 1)
    {
        set_error(ERR_IO, "This file doesn't exist");
        return ERR_IO;
    }

    // open file
    ptr = fopen(filename, "rb");
    if (ptr == NULL)
    {
        set_error(ERR_IO, "Error opening file");
        return ERR_IO;    
    }

    int read = 0;

    // read header parts

    read = fread(header.riff, sizeof(header.riff), 1, ptr);

    read = fread(buffer4, sizeof(buffer4), 1, ptr);

    // convert little endian to big endian 4 byte int
    header.overall_size = buffer4[0] |
                          (buffer4[1] << 8) |
                          (buffer4[2] << 16) |
                          (buffer4[3] << 24);


    read = fread(header.wave, sizeof(header.wave), 1, ptr);

    read = fread(header.fmt_chunk_marker, sizeof(header.fmt_chunk_marker), 1, ptr);

    read = fread(buffer4, sizeof(buffer4), 1, ptr);

    // convert little endian to big endian 4 byte integer
    header.length_of_fmt = buffer4[0] |
                           (buffer4[1] << 8) |
                           (buffer4[2] << 16) |
                           (buffer4[3] << 24);

    read = fread(buffer2, sizeof(buffer2), 1, ptr);

    header.format_type = buffer2[0] | (buffer2[1] << 8);
    char format_name[10] = "";
    if (header.format_type == 1)
        strcpy(format_name, "PCM");
    else if (header.format_type == 6)
        strcpy(format_name, "A-law");
    else if (header.format_type == 7)
        strcpy(format_name, "Mu-law");


    read = fread(buffer2, sizeof(buffer2), 1, ptr);

    header.channels = buffer2[0] | (buffer2[1] << 8);

    read = fread(buffer4, sizeof(buffer4), 1, ptr);

    header.sample_rate = buffer4[0] |
                         (buffer4[1] << 8) |
                         (buffer4[2] << 16) |
                         (buffer4[3] << 24);


    read = fread(buffer4, sizeof(buffer4), 1, ptr);

    header.byterate = buffer4[0] |
                      (buffer4[1] << 8) |
                      (buffer4[2] << 16) |
                      (buffer4[3] << 24);

    read = fread(buffer2, sizeof(buffer2), 1, ptr);

    header.block_align = buffer2[0] |
                         (buffer2[1] << 8);

    read = fread(buffer2, sizeof(buffer2), 1, ptr);

    header.bits_per_sample = buffer2[0] |
                             (buffer2[1] << 8);

    read = fread(header.data_chunk_header, sizeof(header.data_chunk_header), 1, ptr);

    read = fread(buffer4, sizeof(buffer4), 1, ptr);

    header.data_size = buffer4[0] |
                       (buffer4[1] << 8) |
                       (buffer4[2] << 16) |
                       (buffer4[3] << 24);

    // calculate no.of samples
    long num_samples = (8 * header.data_size) / (header.channels * header.bits_per_sample);

    long size_of_each_sample = (header.channels * header.bits_per_sample) / 8;

    // calculate duration of file
    float duration_in_seconds = (float)header.overall_size / header.byterate;

    int sample_data[header.channels];
    int data[num_samples * header.channels];
    char *error_message;

    // read each sample from data chunk if PCM
    if (header.format_type == 1)
    { // PCM

        long i = 0;
        long data_index = 0;
        char data_buffer[size_of_each_sample];
        int size_is_correct = TRUE;

        // make sure that the bytes-per-sample is completely divisible by num.of channels
        long bytes_in_each_channel = (size_of_each_sample / header.channels);
        if ((bytes_in_each_channel * header.channels) != size_of_each_sample)
        {
            sprintf(error_message, "Error wrong file format: %ld x %u <> %ld", bytes_in_each_channel, header.channels, size_of_each_sample);
            set_error(ERR_FORMAT,error_message);
            size_is_correct = FALSE;
            return ERR_FORMAT;
        }

        if (size_is_correct)
        {
            // the valid amplitude range for values based on the bits per sample
            long low_limit = 0l;
            long high_limit = 0l;

            switch (header.bits_per_sample)
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

            printf("\n\nValid range for data values : %ld to %ld\n", low_limit, high_limit);
            for (i = 1; i <= num_samples; i++)
            {
                read = fread(data_buffer, sizeof(data_buffer), 1, ptr);
                if (read == 1)
                {

                    // dump the data read
                    unsigned int xchannels = 0;
                    int data_in_channel = 0;
                    int offset = 0; // move the offset for every iteration in the loop below
                    for (xchannels = 0; xchannels < header.channels; xchannels++)
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
                            set_error(ERR_INTERNAL, "Value out of range")   ; 
                            return ERR_INTERNAL;
                        }

                        data[data_index] = data_in_channel;
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

    } //  if (header.format_type == 1) {

    fclose(ptr);

    *out = header;
    set_error(ERR_OK, NULL);

    return 0;
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


// int main() {

//     struct HEADER *out = (struct HEADER*)malloc(sizeof(struct HEADER));

//     char *filename = "./data/file_example_WAV_2MG.wav";

//     retrieve_wav_data(filename, out);

//     printf("Channels : %u", out->channels);


// }