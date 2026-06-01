#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define BUZZER_ON_DURATION_US 200000

static int write_value(int fd, unsigned char value)
{
	if (write(fd, &value, 1) != 1)
	{
		perror("write");
		return -1;
	}

	return 0;
}

static int read_value(int fd, unsigned char *value)
{
	if (read(fd, value, 1) != 1)
	{
		perror("read");
		return -1;
	}

	return 0;
}

static void usage(const char *prog)
{
	printf("Usage: %s [on|off|pulse|blink] [count] [delay_ms]\n", prog);
	printf("  on      : write 1 to the buzzer and keep it on for 200ms\n");
	printf("  off     : write 0 to the buzzer\n");
	printf("  pulse   : write 1 briefly, then write 0\n");
	printf("  blink   : repeat 1/0 toggling (default count=3, delay_ms=300)\n");
	printf("  tone    : generate a tone: tone <freq_hz> <duration_ms>\n");
	printf("  note    : play musical note: note <note_name> <duration_ms> (e.g. C4, A3, G#5)\n");
	printf("  test    : play C major scale (C4 D4 E4 F4 G4 A4 B4 C5)\n");
}

static int note_name_to_frequency(const char *name, int *out_freq)
{
	if (!name || !out_freq) return -1;

	// Parse note: letter A-G, optional # or b, octave number (e.g. C4, A#3, Db5)
	char note = '\0';
	int accidental = 0; // +1 for sharp, -1 for flat
	int octave = 4; // default

	const char *p = name;
	if (*p >= 'a' && *p <= 'g') note = *p - 'a' + 'A';
	else if (*p >= 'A' && *p <= 'G') note = *p;
	else return -1;
	p++;

	if (*p == '#' || *p == 's' || *p == 'S') { accidental = 1; p++; }
	else if (*p == 'b' || *p == 'B' || *p == 'f' || *p == 'F') { accidental = -1; p++; }

	if (*p == '+' || *p == '-') { // not expected
		return -1;
	}

	if (*p == '\0') return -1;

	// Read octave (may be multi-digit)
	char *endptr = NULL;
	long oct = strtol(p, &endptr, 10);
	if (endptr == p) return -1;
	octave = (int)oct;

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

int main(int argc, char *argv[])
{
	int fd;
	unsigned char value = 0;
	const char *mode = "pulse";
	int count = 3;
	int delay_ms = 300;

	if (argc > 1)
	{
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
		{
			usage(argv[0]);
			return 0;
		}
		mode = argv[1];
	}

	if (argc > 2)
		count = atoi(argv[2]);

	if (argc > 3)
		delay_ms = atoi(argv[3]);

	fd = open(BUZZER_DEVICE, O_RDWR);
	if (fd < 0)
	{
		perror("open");
		fprintf(stderr, "Failed to open %s\n", BUZZER_DEVICE);
		return 1;
	}

	if (strcmp(mode, "on") == 0)
	{
		if (write_value(fd, 1) == 0)
		{
			if (read_value(fd, &value) == 0)
				printf("Buzzer state read back: %u\n", value);
			usleep(BUZZER_ON_DURATION_US);
			write_value(fd, 0);
		}
	}
	else if (strcmp(mode, "off") == 0)
	{
		if (write_value(fd, 0) == 0 && read_value(fd, &value) == 0)
			printf("Buzzer state read back: %u\n", value);
	}
	else if (strcmp(mode, "blink") == 0)
	{
		if (count <= 0)
			count = 3;
		if (delay_ms < 0)
			delay_ms = 300;

		for (int i = 0; i < count; i++)
		{
			write_value(fd, 1);
			usleep((useconds_t)delay_ms * 1000);
			write_value(fd, 0);
			usleep((useconds_t)delay_ms * 1000);
		}
		if (read_value(fd, &value) == 0)
			printf("Buzzer state read back: %u\n", value);
	}
	else if (strcmp(mode, "tone") == 0)
	{
		int freq = 440;
		int duration_ms = 500;

		if (argc > 2) freq = atoi(argv[2]);
		if (argc > 3) duration_ms = atoi(argv[3]);

		if (freq <= 0) freq = 440;
		if (duration_ms <= 0) duration_ms = 500;

		if (freq < 20) freq = 20;
		if (freq > 20000) freq = 20000;

		unsigned int half_period_us = 500000 / (unsigned int)freq; // 1/(2*f) in us

		struct timespec start, now;
		long elapsed_us = 0;
		long target_us = (long)duration_ms * 1000L;

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
	}
	else if (strcmp(mode, "note") == 0)
	{
		if (argc < 3) {
			fprintf(stderr, "note mode requires a note name (e.g. C4)\n");
		} else {
			int freq = 0;
			int duration_ms = 500;
			if (note_name_to_frequency(argv[2], &freq) == 0) {
				if (argc > 3) duration_ms = atoi(argv[3]);
				if (duration_ms <= 0) duration_ms = 500;
				unsigned int half_period_us = 500000 / (unsigned int)freq;
				struct timespec start, now;
				long elapsed_us = 0;
				long target_us = (long)duration_ms * 1000L;
				clock_gettime(CLOCK_MONOTONIC, &start);
				while (elapsed_us < target_us) {
					if (write_value(fd, 1) != 0) break;
					usleep(half_period_us);
					if (write_value(fd, 0) != 0) break;
					usleep(half_period_us);
					clock_gettime(CLOCK_MONOTONIC, &now);
					elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;
				}
				write_value(fd, 0);
			} else {
				fprintf(stderr, "Invalid note name: %s\n", argv[2]);
			}
		}
	}
	else if (strcmp(mode, "test") == 0)
	{
		const char *scale[] = {"C4","D4","E4","F4","G4","A4","B4","C5"};
		int notes = sizeof(scale) / sizeof(scale[0]);
		int duration_ms = 300;
		int gap_ms = 50;
		for (int i = 0; i < notes; ++i) {
			int freq = 0;
			if (note_name_to_frequency(scale[i], &freq) != 0) continue;
			unsigned int half_period_us = 500000 / (unsigned int)freq;
			struct timespec start, now;
			long elapsed_us = 0;
			long target_us = (long)duration_ms * 1000L;
			clock_gettime(CLOCK_MONOTONIC, &start);
			while (elapsed_us < target_us) {
				if (write_value(fd, 1) != 0) break;
				usleep(half_period_us);
				if (write_value(fd, 0) != 0) break;
				usleep(half_period_us);
				clock_gettime(CLOCK_MONOTONIC, &now);
				elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;
			}
			write_value(fd, 0);
			usleep((useconds_t)gap_ms * 1000);
		}
	}
	else
	{
		if (write_value(fd, 1) == 0)
		{
			usleep(150000);
			write_value(fd, 0);
			if (read_value(fd, &value) == 0)
				printf("Buzzer state read back: %u\n", value);
		}
	}

	close(fd);
	return 0;
}