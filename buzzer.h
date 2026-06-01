#ifndef BUZZER_H
#define BUZZER_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// Write a single byte to the buzzer device
int write_value(int fd, unsigned char value);

// Read a single byte from the buzzer device
int read_value(int fd, unsigned char *value);

// Play a square-wave tone on an open buzzer fd
int play_tone(int fd, unsigned int freq, long duration_ms);

// Convert note name (e.g. "A4", "C#3") to frequency in Hz
int note_name_to_frequency(const char *name, int *out_freq);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_H
