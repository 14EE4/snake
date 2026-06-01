#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buzzer.h"

#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define BUZZER_ON_DURATION_US 200000
#define DEFAULT_SEQ_FILE "out.seq"
#define DEFAULT_SEQ_GAP_MS 20

static volatile sig_atomic_t g_stop_requested = 0;
static int g_buzzer_fd = -1;

static void cleanup_buzzer(void)
{
	if (g_buzzer_fd >= 0)
	{
		write_value(g_buzzer_fd, 0);
		close(g_buzzer_fd);
		g_buzzer_fd = -1;
	}
}

static void handle_sigint(int signum)
{
	(void)signum;
	g_stop_requested = 1;
}

static void usage(const char *prog)
{
	printf("Usage: %s [on|off|pulse|blink|tone|note|test|seq]\n", prog);
	printf("  on      : write 1 to the buzzer and keep it on for 200ms\n");
	printf("  off     : write 0 to the buzzer\n");
	printf("  pulse   : write 1 briefly, then write 0\n");
	printf("  blink   : repeat 1/0 toggling (default count=3, delay_ms=300)\n");
	printf("  tone    : generate a tone: tone <freq_hz> <duration_ms>\n");
	printf("  note    : play musical note: note <note_name> <duration_ms> (e.g. C4, A3, G#5)\n");
	printf("  test    : play C major scale (C4 D4 E4 F4 G4 A4 B4 C5)\n");
	printf("  seq     : play a sequence file: seq [path]\n");
}

// note_name_to_frequency and play_tone are provided in buzzer.c via buzzer.h

static int play_sequence_file(int fd, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
	{
		perror("fopen");
		fprintf(stderr, "Failed to open sequence file: %s\n", path);
		return -1;
	}

	char line[128];
	int line_no = 0;
	while (fgets(line, sizeof(line), fp))
	{
		if (g_stop_requested)
			break;

		line_no++;
		char *p = line;
		while (*p && isspace((unsigned char)*p)) p++;
		if (*p == '\0' || *p == '#')
			continue;

		char *comma = strchr(p, ',');
		char *comma2 = comma ? strchr(comma + 1, ',') : NULL;
		if (!comma || !comma2)
		{
			fprintf(stderr, "Skipping malformed line %d: %s", line_no, line);
			continue;
		}

		*comma = '\0';
		*comma2 = '\0';

		char *midi_end = NULL;
		char *rest_end = NULL;
		char *duration_end = NULL;
		long midi_note = strtol(p, &midi_end, 10);
		double rest_ms = strtod(comma + 1, &rest_end);
		double duration_ms = strtod(comma2 + 1, &duration_end);
		if (midi_end == p || rest_end == comma + 1 || duration_end == comma2 + 1)
		{
			fprintf(stderr, "Skipping malformed line %d: %s", line_no, line);
			continue;
		}

		int freq = 0;
		if (midi_note_to_frequency((int)midi_note, &freq) != 0)
		{
			fprintf(stderr, "Skipping unsupported MIDI note %ld on line %d\n", midi_note, line_no);
			continue;
		}

		if (rest_ms > 0.0)
		{
			if (usleep((useconds_t)(rest_ms * 1000.0 + 0.5)) != 0 && errno == EINTR)
				break;
		}

		if (duration_ms <= 0.0)
			duration_ms = 100.0;

		play_tone_seconds(fd, (unsigned int)freq, duration_ms / 1000.0);
		if (g_stop_requested)
			break;
	}

	fclose(fp);
	return 0;
}

int main(int argc, char *argv[])
{
	int fd;
	unsigned char value = 0;
	const char *mode = "pulse";
	struct sigaction sa;

	if (argc > 1)
	{
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
		{
			usage(argv[0]);
			return 0;
		}
		mode = argv[1];
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_sigint;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	g_stop_requested = 0;

	fd = open(BUZZER_DEVICE, O_RDWR);
	if (fd < 0)
	{
		perror("open");
		fprintf(stderr, "Failed to open %s\n", BUZZER_DEVICE);
		return 1;
	}

	g_buzzer_fd = fd;

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
		int count = 3;
		int delay_ms = 300;

		if (argc > 2)
			count = atoi(argv[2]);
		if (argc > 3)
			delay_ms = atoi(argv[3]);

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
	else if (strcmp(mode, "seq") == 0)
	{
		const char *path = (argc > 2) ? argv[2] : DEFAULT_SEQ_FILE;
		(void)argc;
		play_sequence_file(fd, path);
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

		play_tone(fd, (unsigned int)freq, duration_ms);
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
				play_tone(fd, (unsigned int)freq, duration_ms);
			} else {
				fprintf(stderr, "Invalid note name: %s\n", argv[2]);
			}
		}
	}
	else if (strcmp(mode, "test") == 0)
	{
		const char *scale[] = {"C4","D4","E4","F4","G4","A4","B4","C5"};
		int notes = sizeof(scale) / sizeof(scale[0]);
		int duration_ms = 100;
		int gap_ms = 5;
		for (int i = 0; i < notes; ++i) {
			int freq = 0;
			if (note_name_to_frequency(scale[i], &freq) != 0) continue;
			play_tone(fd, (unsigned int)freq, duration_ms);
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

	cleanup_buzzer();
	g_buzzer_fd = -1;
	return 0;
}