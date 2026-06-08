// 헤더 파일
#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

using namespace std;

#include "buzzer.h"

// 게임 보드 가로/세로 크기
const int width = 80;
const int height = 20;

// FPGA 푸시 스위치 디바이스
int fd_fpga_switch = -1;
bool use_fpga_switch = false;

// FPGA FND 디바이스
int fd_fpga_fnd = -1;

// 인터럽트 스위치 디바이스
int fd_sw = -1;
bool use_interrupt_switch = false;
// 버저 디바이스
int fd_buzzer = -1;

// 뱀 머리 좌표 (x축, y축)
int x, y;
// 벽 통과 모드 여부 (true: 반대편 출현, false: 게임 오버)
bool wrapWalls = false;
// 일반 과일 좌표
int fruitCordX, fruitCordY;
// 느린 과일(*) 좌표 및 상태
bool slowFruitActive = false;
int slowFruitX, slowFruitY;
int slowEffectTicks = 0;
const int SLOW_DURATION = 20;
// 플레이어 점수
int playerScore;
// 최고 점수
int highScore = 0;
string highScoreName = "";
// 뱀 꼬리 좌표 배열 (x축, y축)
int snakeTailX[100], snakeTailY[100];
// 뱀 꼬리 길이
int snakeTailLen;
// 뱀 이동 방향
enum snakesDirection
{
	STOP = 0,
	LEFT,
	RIGHT,
	UP,
	DOWN
};
// 현재 이동 방향 변수
snakesDirection sDir;
// 게임 오버 여부
bool isGameOver;

bool gamePaused = false;
bool pauseMenuActive = false;

void disableRawMode();
void enableRawMode();

void UpdateFNDScore(int score)
{
	if (fd_fpga_fnd < 0)
		return;

	unsigned char fnd_value[4];
	if (score < 0)
		score = 0;
	if (score > 9999)
		score = 9999;

	fnd_value[0] = (score / 1000) % 10;
	fnd_value[1] = (score / 100) % 10;
	fnd_value[2] = (score / 10) % 10;
	fnd_value[3] = score % 10;

	write(fd_fpga_fnd, fnd_value, 4);
}

static void PlayBuzzerNote(const char *note_name, long duration_ms)
{
	if (fd_buzzer < 0)
		return;

	int freq = 0;
	if (note_name_to_frequency(note_name, &freq) == 0)
		play_tone(fd_buzzer, (unsigned int)freq, duration_ms);
}

static void PlayStartSound()
{
	PlayBuzzerNote("G5", 90);
	PlayBuzzerNote("B5", 90);
}

static void PlayGameOverSound()
{
	PlayBuzzerNote("G4", 120);
	PlayBuzzerNote("D4", 120);
	PlayBuzzerNote("G3", 180);
}

// 게임 변수 초기화 함수
void GameInit()
{
	isGameOver = false;
	gamePaused = false;
	sDir = STOP;
	x = width / 2;
	y = height / 2;
	fruitCordX = rand() % width;
	fruitCordY = rand() % height;
	playerScore = 0;
	snakeTailLen = 0;
	slowFruitActive = false;
	slowEffectTicks = 0;
	memset(snakeTailX, 0, sizeof(snakeTailX));
	memset(snakeTailY, 0, sizeof(snakeTailY));
	UpdateFNDScore(playerScore);
}

// 난이도 선택 함수
unsigned int SelectDifficulty()
{
	cout << endl;
	cout << "=== Select Difficulty ===" << endl;

	if (use_fpga_switch)
	{
		cout << "Button 0 = Easy   (200ms - 느리게)" << endl;
		cout << "Button 1 = Normal (150ms - 보통)" << endl;
		cout << "Button 2 = Hard   (100ms - 빠르게)" << endl;

		// 모든 버튼이 떼어질 때까지 대기
		unsigned char sw_state[13];
		bool anyPressed = true;
		while (anyPressed)
		{
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				anyPressed = false;
				for (int i = 0; i < 13; i++)
				{
					if (sw_state[i])
					{
						anyPressed = true;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		while (true)
		{
			unsigned char sw_state[13];
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				if (sw_state[0])
				{
					cout << ">> Easy 선택!" << endl;
					PlayBuzzerNote("A5", 90);
					sleep(1);
					return 200;
				}
				if (sw_state[1])
				{
					cout << ">> Normal 선택!" << endl;
					PlayBuzzerNote("C5", 90);
					sleep(1);
					return 150;
				}
				if (sw_state[2])
				{
					cout << ">> Hard 선택!" << endl;
					PlayBuzzerNote("E5", 90);
					sleep(1);
					return 100;
				}
			}
		}
	}
	else
	{
		cout << "0 = Easy   (200ms - 느리게)" << endl;
		cout << "1 = Normal (150ms - 보통)" << endl;
		cout << "2 = Hard   (100ms - 빠르게)" << endl;
		cout << "선택: " << flush;

		char choice;
		while (true)
		{
			cin >> choice;
			if (choice == '0')
			{
				cout << ">> Easy 선택!" << endl;
				PlayBuzzerNote("A5", 90);
				return 200;
			}
			if (choice == '1')
			{
				cout << ">> Normal 선택!" << endl;
				PlayBuzzerNote("C5", 90);
				return 150;
			}
			if (choice == '2')
			{
				cout << ">> Hard 선택!" << endl;
				return 100;
			}
			cout << "0, 1, 2 중에 선택해주세요: " << flush;
		}
	}
}

// 벽 동작 모드 선택 함수
bool SelectMode()
{
	cout << endl;
	cout << "=== Select Mode ===" << endl;

	if (use_fpga_switch)
	{
		cout << "Button 0 = Normal (벽 닿으면 죽음)" << endl;
		cout << "Button 1 = Wrap   (벽 닿으면 반대편으로 넘어감)" << endl;

		// 버튼 릴리즈 대기
		unsigned char sw_state[13];
		bool anyPressed = true;
		while (anyPressed)
		{
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				anyPressed = false;
				for (int i = 0; i < 13; i++)
				{
					if (sw_state[i])
					{
						anyPressed = true;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		while (true)
		{
			unsigned char sw_state[13];
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				if (sw_state[0])
				{
					cout << ">> Normal 모드 선택!" << endl;
					PlayBuzzerNote("A4", 120);
					sleep(1);
					return false;
				}
				if (sw_state[1])
				{
					cout << ">> Wrap 모드 선택!" << endl;
					PlayBuzzerNote("C4", 120);
					sleep(1);
					return true;
				}
			}
		}
	}
	else
	{
		cout << "0 = Normal (벽 닿으면 죽음)" << endl;
		cout << "1 = Wrap   (벽 닿으면 반대편으로 넘어감)" << endl;
		cout << "선택: " << flush;

		char choice;
		while (true)
		{
			cin >> choice;
			if (choice == '0')
			{
				cout << ">> Normal 모드 선택!" << endl;
				return false;
			}
			if (choice == '1')
			{
				cout << ">> Wrap 모드 선택!" << endl;
				return true;
			}
			cout << "0 또는 1 중에 선택해주세요: " << flush;
		}
	}
}

bool PromptRestart()
{
	disableRawMode();
	string currentPlayerName;

	PlayGameOverSound();

	cout << "Enter your name to record this score: ";
	cin >> currentPlayerName;
	cout << endl;

	cout << "Game Over! Final Score: " << playerScore << endl;

	// 최고 점수 갱신 확인
	if (playerScore > highScore)
	{
		highScore = playerScore;
		highScoreName = currentPlayerName;
		cout << "*** NEW HIGH SCORE! ***" << endl;
	}
	else
	{
		cout << "Current Score: " << playerScore << " (" << currentPlayerName << ")" << endl;
		cout << "High Score: " << highScore << " (" << highScoreName << ")" << endl;
	}
	cout << endl;

	if (use_fpga_switch)
	{
		cout << "Restart? FPGA Switch 0 = restart, 2 = exit" << endl;
	}
	else
	{
		cout << "Restart? (y/n): " << flush;
	}

	while (true)
	{
		if (use_fpga_switch)
		{
			unsigned char sw_state[13];
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				if (sw_state[0])
				{
					PlayStartSound();
					return true;
				}
				if (sw_state[2])
				{
					PlayGameOverSound();
					return false;
				}
			}
		}
		else
		{
			char choice;
			if (!(cin >> choice))
			{
				return false;
			}

			if (choice == 'y' || choice == 'Y')
			{
				return true;
			}

			if (choice == 'n' || choice == 'N')
			{
				PlayGameOverSound();
				return false;
			}

			cout << "Please enter y or n." << endl;
		}
	}
}

// 게임 보드 렌더링 함수
void GameRender(string playerName, unsigned int frameMs)
{
	std::system("clear"); // 화면 지우기

	// 상단 벽 '-' 출력
	for (int i = 0; i < width + 2; i++)
		cout << "-";
	cout << endl;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j <= width; j++)
		{
			// 좌우 벽 '|' 출력
			if (j == 0 || j == width)
				cout << "|";
			// 뱀 머리 'O' 출력
			if (i == y && j == x)
				cout << "O";
			// 일반 과일 '#' 출력
			else if (i == fruitCordY && j == fruitCordX)
				cout << "#";
			// 느린 과일 '*' 출력
			else if (slowFruitActive && i == slowFruitY && j == slowFruitX)
				cout << "*";
			else
			{
				bool prTail = false;
				for (int k = 0; k < snakeTailLen; k++)
				{
					if (snakeTailX[k] == j && snakeTailY[k] == i)
					{
						cout << "o";
						prTail = true;
					}
				}
				if (!prTail)
					cout << " ";
			}
		}
		cout << endl;
	}

	// 하단 벽 '-' 출력
	for (int i = 0; i < width + 2; i++)
		cout << "-";
	cout << endl;

	// 점수 및 난이도 표시
	cout << playerName << "'s Score: " << playerScore;
	if (highScore > 0)
		cout << "  |  High Score: " << highScore << " (" << highScoreName << ")";
	cout << endl;

	// 난이도 표시
	string diffLabel = (frameMs == 200) ? "Easy" : (frameMs == 150) ? "Normal"
																	: "Hard";
	cout << "Difficulty: " << diffLabel << endl;

	if (slowEffectTicks > 0)
		cout << "** SLOW! (" << slowEffectTicks << " ticks 남음) **" << endl;
	cout << "Fruit: # = +10pts  |  * = +5pts + 속도 감소" << endl;

	// 모드 표시
	string modeLabel = wrapWalls ? "Wrap (벽 닿으면 반대편으로)" : "Normal (벽 닿으면 죽음)";
	cout << "Mode: " << modeLabel << endl;

	if (gamePaused)
		cout << "Game Paused: Interrupt switch to resume" << endl;
	if (use_fpga_switch)
		cout << "Control: Button 1=Up / 3=Left / 5=Right / 7=Down (FPGA Switch)" << endl;
	else if (use_interrupt_switch)
		cout << "Control: Interrupt Switch (press to pause/resume)" << endl;
	else
		cout << "Control: Up: 8 / Down: 2 / Left: 4 / Right: 6 (Keyboard)" << endl;
}

// 게임 상태 업데이트 함수
void UpdateGame()
{
	int headX = x;
	int headY = y;
	int prevX = snakeTailX[0];
	int prevY = snakeTailY[0];
	int prev2X, prev2Y;

	switch (sDir)
	{
	case LEFT:
		x--;
		if (x < 0)
		{
			if (wrapWalls)
				x = width - 1;
			else
			{
				PlayBuzzerNote("A2", 300);
				isGameOver = true;
				return;
			}
		}
		break;
	case RIGHT:
		x++;
		if (x >= width)
		{
			if (wrapWalls)
				x = 0;
			else
			{
				PlayBuzzerNote("A2", 300);
				isGameOver = true;
				return;
			}
		}
		break;
	case UP:
		y--;
		if (y < 0)
		{
			if (wrapWalls)
				y = height - 1;
			else
			{
				PlayBuzzerNote("A2", 300);
				isGameOver = true;
				return;
			}
		}
		break;
	case DOWN:
		y++;
		if (y >= height)
		{
			if (wrapWalls)
				y = 0;
			else
			{
				PlayBuzzerNote("A2", 300);
				isGameOver = true;
				return;
			}
		}
		break;
	}

	snakeTailX[0] = headX;
	snakeTailY[0] = headY;

	for (int i = 1; i < snakeTailLen; i++)
	{
		prev2X = snakeTailX[i];
		prev2Y = snakeTailY[i];
		snakeTailX[i] = prevX;
		snakeTailY[i] = prevY;
		prevX = prev2X;
		prevY = prev2Y;
	}

	// 꼬리 충돌 확인
	for (int i = 0; i < snakeTailLen; i++)
	{
		if (snakeTailX[i] == x && snakeTailY[i] == y)
		{
			PlayBuzzerNote("A2", 400);
			isGameOver = true;
		}
	}

	// 일반 과일('#') 충돌 확인
	if (x == fruitCordX && y == fruitCordY)
	{
		playerScore += 10;
		snakeTailLen++;
		UpdateFNDScore(playerScore);
		fruitCordX = rand() % width;
		fruitCordY = rand() % height;
		// 30% 확률로 slow fruit 생성
		if (!slowFruitActive && (rand() % 10) < 3)
		{
			slowFruitActive = true;
			slowFruitX = rand() % width;
			slowFruitY = rand() % height;
		}
		// 과일 획득 효과음 재생
		if (fd_buzzer >= 0)
		{
			PlayBuzzerNote("A5", 100);
			PlayBuzzerNote("C#6", 80);
		}
	}

	// 느린 과일('*') 충돌 확인
	if (slowFruitActive && x == slowFruitX && y == slowFruitY)
	{
		playerScore += 5;
		snakeTailLen++;
		UpdateFNDScore(playerScore);
		slowFruitActive = false;
		slowEffectTicks = SLOW_DURATION;
		// 느린 과일 효과음 재생 (낮은 음)
		PlayBuzzerNote("E5", 140);
	}

	// 속도 감소 효과 틱 감소
	if (slowEffectTicks > 0)
		slowEffectTicks--;
}

// 키보드 입력 처리 함수
void UserInput()
{
	fd_set set;
	struct timeval tv;
	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0)
	{
		int key = getchar();
		switch (key)
		{
		case '4':
			sDir = LEFT;
			break;
		case '6':
			sDir = RIGHT;
			break;
		case '8':
			sDir = UP;
			break;
		case '2':
			sDir = DOWN;
			break;
		case '0':
			isGameOver = true;
			break;
		}
	}
}

// FPGA 푸시 스위치 입력 처리 함수
void FPGASwitchInput()
{
	if (fd_fpga_switch < 0)
		return;

	unsigned char sw_state[13];
	if (read(fd_fpga_switch, sw_state, 13) > 0)
	{
		// 버튼 매핑: 1=위, 3=왼쪽, 5=오른쪽, 7=아래
		if (sw_state[1]){
			PlayBuzzerNote("G4", 120);
			sDir = UP;
		}

		if (sw_state[3]){
			PlayBuzzerNote("C4", 120);
			sDir = LEFT;
		}

		if (sw_state[5]){
			PlayBuzzerNote("E4", 120);
			sDir = RIGHT;
		}
		if (sw_state[7]){
			PlayBuzzerNote("D4", 120);
			sDir = DOWN;
		}

	}
}

// 인터럽트 스위치 입력 처리 함수
void InterruptSwitchInput()
{
	if (fd_sw < 0)
		return;

	static unsigned char prev_state = 0;
	unsigned char sw_state = 0;

	if (read(fd_sw, &sw_state, 1) > 0)
	{
		if (sw_state == 1 && prev_state == 0)
		{
			gamePaused = !gamePaused;
		}
		prev_state = sw_state;
	}
}

// 인터럽트로 일시정지 시 일회성 메뉴 표시
// 재개 시 true, 종료 시 false 반환
bool PauseMenu()
{
	disableRawMode();

	cout << "\n=== Game Paused ===" << endl;
	PlayGameOverSound();
	if (use_fpga_switch)
	{
		cout << "FPGA: Button 0 = Resume, Button 2 = Exit" << endl;
	}
	if (use_interrupt_switch)
	{
		cout << "Interrupt Switch: toggle back to resume" << endl;
	}
	if (!use_fpga_switch && !use_interrupt_switch)
	{
		cout << "Keyboard: r = resume" << endl;
	}

	unsigned char prev_fpga[13] = {0};
	bool prev_interrupt_state = false;
	if (fd_sw >= 0)
	{
		unsigned char sw_state = 0;
		if (read(fd_sw, &sw_state, 1) > 0)
		{
			prev_interrupt_state = (sw_state != 0);
		}
	}
	if (fd_fpga_switch >= 0)
	{
		unsigned char sw_state[13];
		if (read(fd_fpga_switch, sw_state, 13) > 0)
		{
			memcpy(prev_fpga, sw_state, 13);
		}
	}

	while (true)
	{
		fd_set set;
		FD_ZERO(&set);
		int maxFd = -1;

		if (fd_fpga_switch >= 0)
		{
			FD_SET(fd_fpga_switch, &set);
			if (fd_fpga_switch > maxFd)
				maxFd = fd_fpga_switch;
		}

		if (fd_sw >= 0)
		{
			FD_SET(fd_sw, &set);
			if (fd_sw > maxFd)
				maxFd = fd_sw;
		}

		FD_SET(STDIN_FILENO, &set);
		if (STDIN_FILENO > maxFd)
			maxFd = STDIN_FILENO;

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		int ready = select(maxFd + 1, &set, NULL, NULL, &tv);
		if (ready > 0)
		{
			if (fd_fpga_switch >= 0 && FD_ISSET(fd_fpga_switch, &set))
			{
				unsigned char sw_state[13];
				if (read(fd_fpga_switch, sw_state, 13) > 0)
				{
					//continue 
					if (sw_state[0] && !prev_fpga[0])
					{
						PlayStartSound();
						enableRawMode();
						return true;
					}
					//game over if button 2 is pressed
					if (sw_state[2] && !prev_fpga[2])
					{
						PlayGameOverSound();
						return false;
					}
					memcpy(prev_fpga, sw_state, 13);
				}
			}

			if (fd_sw >= 0 && FD_ISSET(fd_sw, &set))
			{
				unsigned char sw_state = 0;
				if (read(fd_sw, &sw_state, 1) > 0)
				{
					bool current_interrupt_state = (sw_state != 0);
					if (current_interrupt_state && !prev_interrupt_state)
					{
						enableRawMode();
						return true;
					}
					prev_interrupt_state = current_interrupt_state;
				}
			}

			if (FD_ISSET(STDIN_FILENO, &set))
			{
				int c = getchar();
				if (c == 'r' || c == 'R')
				{
					enableRawMode();
					return true;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

static struct termios orig_termios;

void disableRawMode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		return;
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	atexit(disableRawMode);
}

// 메인 함수 / 게임 루프
int main()
{
	// FPGA 푸시 스위치 디바이스 열기
	fd_fpga_switch = open("/dev/fpga_push_switch", O_RDWR);
	if (fd_fpga_switch >= 0)
	{
		use_fpga_switch = true;
		cout << "FPGA push switch device opened successfully!" << endl;
	}
	else
	{
		cout << "FPGA push switch device not found. Using keyboard input." << endl;
	}

	// FPGA FND 디바이스 열기
	fd_fpga_fnd = open("/dev/fpga_fnd", O_RDWR);
	if (fd_fpga_fnd >= 0)
	{
		cout << "FPGA FND device opened successfully!" << endl;
	}
	else
	{
		cout << "FPGA FND device not found. Score will be shown on console only." << endl;
	}

	// 인터럽트 스위치 디바이스 열기
	fd_sw = open("/dev/my_led_dev", O_RDWR);
	if (fd_sw >= 0)
	{
		use_interrupt_switch = true;
		cout << "Interrupt switch device opened successfully!" << endl;
	}
	else
	{
		cout << "Interrupt switch device not found." << endl;
	}

	// 버저 디바이스 열기 (선택)
	fd_buzzer = open("/dev/fpga_buzzer", O_RDWR);
	if (fd_buzzer >= 0)
	{
		cout << "FPGA buzzer device opened successfully!" << endl;
	}
	else
	{
		cout << "FPGA buzzer device not found (sound disabled)." << endl;
	}

	GameInit();

	// 게임 시작 전 난이도 및 모드 선택
	unsigned int frameMs = SelectDifficulty();
	wrapWalls = SelectMode();
	enableRawMode();
	PlayStartSound();

	while (true)
	{
		while (!isGameOver)
		{
			unsigned int currentFrameMs = (slowEffectTicks > 0) ? frameMs * 2 : frameMs;
			GameRender("Player", frameMs);

			// 인터럽트 스위치 우선 확인 (일시정지/재개)
			if (use_interrupt_switch)
			{
				InterruptSwitchInput();

				// 일시정지 시 일회성 메뉴 표시 (반복 출력 방지)
				if (gamePaused && !pauseMenuActive)
				{
					pauseMenuActive = true;
					bool resume = PauseMenu();
					pauseMenuActive = false;
					if (!resume)
					{
						isGameOver = true;
						break;
					}
					else
					{
						gamePaused = false;
						continue;
					}
				}
			}

			// 일시정지 중이 아닐 때만 이동 입력 처리
			if (!gamePaused && use_fpga_switch)
			{
				FPGASwitchInput();
			}
			else if (!gamePaused)
			{
				UserInput();
			}

			if (!gamePaused)
			{
				UpdateGame();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(currentFrameMs));
		}

		if (!PromptRestart())
		{
			break;
		}
		GameInit();

		// 재시작 시 난이도 및 모드 재선택
		frameMs = SelectDifficulty();
		wrapWalls = SelectMode();
		enableRawMode();
		PlayStartSound();
	}

	// 열린 디바이스 닫기
	if (fd_fpga_switch >= 0)
		close(fd_fpga_switch);
	if (fd_fpga_fnd >= 0)
		close(fd_fpga_fnd);
	if (fd_sw >= 0)
		close(fd_sw);
	if (fd_buzzer >= 0)
		close(fd_buzzer);

	return 0;
}