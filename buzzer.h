#ifndef BUZZER_H
#define BUZZER_H

#include <unistd.h>

// Write a single byte to the buzzer device
int write_value(int fd, unsigned char value);

// Read a single byte from the buzzer device
int read_value(int fd, unsigned char *value);

// Play a square-wave tone on an open buzzer fd
int play_tone(int fd, unsigned int freq, long duration_ms);

// Play a square-wave tone for a fractional duration in seconds
int play_tone_seconds(int fd, unsigned int freq, double duration_s);

// Convert note name (e.g. "A4", "C#3") to frequency in Hz
int note_name_to_frequency(const char *name, int *out_freq);

// Convert a MIDI note number (12=C0, 69=A4) to frequency in Hz
int midi_note_to_frequency(int midi_note, int *out_freq);

#endif // BUZZER_H
