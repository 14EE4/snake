#include "buzzer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

// Deterministic lookup table for equal-tempered notes.
// Rows: C0..B8, Columns: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
static const int frequency_table[9][12] = {
    {16, 17, 18, 19, 21, 22, 23, 25, 26, 28, 29, 31},
    {33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62},
    {65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123},
    {131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247},
    {262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494},
    {523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988},
    {1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976},
    {2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951},
    {4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902}
};

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
    return play_tone_seconds(fd, freq, (double)duration_ms / 1000.0);
}

int play_tone_seconds(int fd, unsigned int freq, double duration_s)
{
    if (freq < 20 || freq > 20000) return -1;
    if (duration_s <= 0.0) return -1;

    unsigned int half_period_us = 500000 / freq; // 1/(2*f) in us
    struct timespec start, now;
    long elapsed_us = 0;
    long target_us = (long)(duration_s * 1000000.0 + 0.5);

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (elapsed_us < target_us)
    {
        if (write_value(fd, 1) != 0) break;
        if (usleep(half_period_us) != 0 && errno == EINTR) break;
        if (write_value(fd, 0) != 0) break;
        if (usleep(half_period_us) != 0 && errno == EINTR) break;

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;
    }

    write_value(fd, 0);
    return 0;
}

int note_name_to_frequency(const char *name, int *out_freq)
{
    if (!name || !out_freq) return -1;

    const char *p = name;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return -1;

    char c = *p;
    if (c >= 'a' && c <= 'g') c = c - 'a' + 'A';
    if (c < 'A' || c > 'G') return -1;
    char note = c;
    p++;

    int accidental = 0;
    if (*p == '#' || *p == 's' || *p == 'S') { accidental = 1; p++; }
    else if (*p == 'b' || *p == 'B' || *p == 'f' || *p == 'F') { accidental = -1; p++; }

    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return -1;

    char *endptr = NULL;
    long oct = strtol(p, &endptr, 10);
    if (endptr == p) return -1;
    int octave = (int)oct;

    int note_index = -1;
    switch (note) {
        case 'C': note_index = 0; break;
        case 'D': note_index = 2; break;
        case 'E': note_index = 4; break;
        case 'F': note_index = 5; break;
        case 'G': note_index = 7; break;
        case 'A': note_index = 9; break;
        case 'B': note_index = 11; break;
        default: return -1;
    }

    note_index += accidental;

    // 옥타브 보정
    if (note_index < 0) {
        note_index += 12;
        octave -= 1;
    }
    if (note_index >= 12) {
        note_index -= 12;
        octave += 1;
    }

    if (octave < 0 || octave > 8) return -1;

    *out_freq = frequency_table[octave][note_index];
    return 0;

}
