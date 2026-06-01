#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUZZER_DEVICE "/dev/fpga_buzzer"

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
	printf("  on      : write 1 to the buzzer and keep it on for one second\n");
	printf("  off     : write 0 to the buzzer\n");
	printf("  pulse   : write 1 briefly, then write 0\n");
	printf("  blink   : repeat 1/0 toggling (default count=3, delay_ms=300)\n");
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
			sleep(1);
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