#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h> // bool, true, false 정의
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "buzzer.h"

#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define BUZZER_ON_DURATION_US 200000
#define DEFAULT_SEQ_FILE "out.seq"
#define DEFAULT_SEQ_GAP_MS 20

static volatile sig_atomic_t g_stop_requested = 0;
static int g_buzzer_fd = -1;

// FPGA 푸시 스위치 디바이스
int fd_fpga_switch = -1;
bool use_fpga_switch = false;
// 버저 디바이스
int fd_buzzer = -1;

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


// note_name_to_frequency and play_tone are provided in buzzer.c via buzzer.h

static void PlayBuzzerNote(const char *note_name, long duration_ms)
{
	if (fd_buzzer < 0)
		return;

	int freq = 0;
	if (note_name_to_frequency(note_name, &freq) == 0)
		play_tone(fd_buzzer, (unsigned int)freq, duration_ms);
}

// FPGA 푸시 스위치 입력 처리 함수
bool FPGASwitchInput()
{
	const char *scale[] = {"C4","D4","E4","F4","G4","A4","B4","C5"};
	if (fd_fpga_switch < 0)
		return;

	unsigned char sw_state[13];
	if (read(fd_fpga_switch, sw_state, 13) > 0)
	{
		

		if (sw_state[0])
			PlayBuzzerNote("C4", 120);
		if (sw_state[1])
			PlayBuzzerNote("D4", 120);
		if (sw_state[2])
			PlayBuzzerNote("E4", 120);
		if (sw_state[3])
			PlayBuzzerNote("F4", 120);
		if (sw_state[4])
			PlayBuzzerNote("G4", 120);
		if (sw_state[5])
			PlayBuzzerNote("A4", 120);
		if (sw_state[6])
			PlayBuzzerNote("B4", 120);
		if (sw_state[7])
			PlayBuzzerNote("C5", 120);
		if (sw_state[8])
			return false;
	}
}

int main(int argc, char *argv[])
{
	int fd;
	unsigned char value = 0;

	struct sigaction sa;



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


	while(FPGASwitchInput())
	{
		

	}


	cleanup_buzzer();
	g_buzzer_fd = -1;
	return 0;
}