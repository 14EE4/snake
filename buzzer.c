#include "buzzer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

int write_value(int fd, unsigned char value)
{
    if (write(fd, &value, 1) != 1)
    {
        perror("write");
        return -1;
    }

    return 0;
}

int read_value(int fd, unsigned char *value)
{
    if (read(fd, value, 1) != 1)
    {
        perror("read");
        return -1;
    }

    return 0;
}

int play_tone(int fd, unsigned int freq, long duration_ms)
{
    if (freq < 20 || freq > 20000) return -1;

    unsigned int half_period_us = 500000 / freq; // 1/(2*f) in us
    struct timespec start, now;
    long elapsed_us = 0;
    long target_us = duration_ms * 1000L;

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (elapsed_us < target_us)
    {
        if (write_value(fd, 1) != 0) break;
        usleep(half_period_us);
        if (write_value(fd, 0) != 0) break;
        usleep(half_period_us);

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;
    }

    write_value(fd, 0);
    return 0;
}

int note_name_to_frequency(const char *name, int *out_freq)
{
    if (!name || !out_freq) return -1;

    // Robust parse: skip leading whitespace, accept note letter, optional accidental, then octave digits
    const char *p = name;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return -1;

    char c = *p;
    if (c >= 'a' && c <= 'g') c = c - 'a' + 'A';
    if (c < 'A' || c > 'G') return -1;
    char note = c;
    p++;

    int accidental = 0; // +1 for sharp, -1 for flat
    if (*p == '#' || *p == 's' || *p == 'S') { accidental = 1; p++; }
    else if (*p == 'b' || *p == 'B' || *p == 'f' || *p == 'F') { accidental = -1; p++; }

    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return -1;

    // Read octave (may be multi-digit, allow optional sign)
    char *endptr = NULL;
    long oct = strtol(p, &endptr, 10);
    if (endptr == p) return -1;
    int octave = (int)oct;

    // Map note letter to semitone offset relative to A in the same octave
    int semitone_from_A = 0;
    switch (note) {
        case 'C': semitone_from_A = -9; break;
        case 'D': semitone_from_A = -7; break;
        case 'E': semitone_from_A = -5; break;
        case 'F': semitone_from_A = -4; break;
        case 'G': semitone_from_A = -2; break;
        case 'A': semitone_from_A = 0;  break;
        case 'B': semitone_from_A = 2;  break;
        default: return -1;
    }

    semitone_from_A += accidental;

    // Compute semitone distance from A4
    int semitone_distance = semitone_from_A + (octave - 4) * 12;

    double freq = 440.0 * pow(2.0, semitone_distance / 12.0);
    if (freq < 1.0) return -1;
    *out_freq = (int)(freq + 0.5);
    return 0;
}
