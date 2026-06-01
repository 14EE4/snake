// required header file
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

// height and width of the boundary
const int width = 80;
const int height = 20;

// FPGA push switch device
int fd_fpga_switch = -1;
bool use_fpga_switch = false;

// FPGA FND device
int fd_fpga_fnd = -1;

// Interrupt switch device
int fd_sw = -1;
bool use_interrupt_switch = false;

// Snake head coordinates of snake (x-axis, y-axis)
int x, y;
// Whether hitting wall wraps to opposite side (true) or causes death (false)
bool wrapWalls = false;
// Food coordinates
int fruitCordX, fruitCordY;
// Slow fruit (*) coordinates and state
bool slowFruitActive = false;
int slowFruitX, slowFruitY;
int slowEffectTicks = 0;
const int SLOW_DURATION = 20;
// variable to store the score of the player
int playerScore;
// variable to store the highest score
int highScore = 0;
string highScoreName = "";
// Array to store the coordinates of snake tail (x-axis, y-axis)
int snakeTailX[100], snakeTailY[100];
// variable to store the length of the snake's tail
int snakeTailLen;
// for storing snake's moving snakesDirection
enum snakesDirection
{
	STOP = 0,
	LEFT,
	RIGHT,
	UP,
	DOWN
};
// snakesDirection variable
snakesDirection sDir;
// boolean variable for checking game is over or not
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

// Function to initialize game variables
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

// Function to select difficulty
unsigned int SelectDifficulty()
{
	cout << endl;
	cout << "=== Select Difficulty ===" << endl;

	if (use_fpga_switch)
	{
		cout << "Button 0 = Easy   (200ms - 느리게)" << endl;
		cout << "Button 1 = Normal (150ms - 보통)" << endl;
		cout << "Button 2 = Hard   (100ms - 빠르게)" << endl;

		// Wait for all buttons to be released before reading input
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
					sleep(1);
					return 200;
				}
				if (sw_state[1])
				{
					cout << ">> Normal 선택!" << endl;
					sleep(1);
					return 150;
				}
				if (sw_state[2])
				{
					cout << ">> Hard 선택!" << endl;
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
				return 200;
			}
			if (choice == '1')
			{
				cout << ">> Normal 선택!" << endl;
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

// Function to select wall behavior mode
bool SelectMode()
{
	cout << endl;
	cout << "=== Select Mode ===" << endl;

	if (use_fpga_switch)
	{
		cout << "Button 0 = Normal (벽 닿으면 죽음)" << endl;
		cout << "Button 1 = Wrap   (벽 닿으면 반대편으로 넘어감)" << endl;

		// Wait for release
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
					sleep(1);
					return false;
				}
				if (sw_state[1])
				{
					cout << ">> Wrap 모드 선택!" << endl;
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

	cout << "Enter your name to record this score: ";
	cin >> currentPlayerName;
	cout << endl;

	cout << "Game Over! Final Score: " << playerScore << endl;

	// Check and update high score
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
					return true;
				}
				if (sw_state[2])
				{
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
				return false;
			}

			cout << "Please enter y or n." << endl;
		}
	}
}

// Function for creating the game board & rendering
void GameRender(string playerName, unsigned int frameMs)
{
	std::system("clear"); // Clear the console (POSIX)

	// Creating top walls with '-'
	for (int i = 0; i < width + 2; i++)
		cout << "-";
	cout << endl;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j <= width; j++)
		{
			// Creating side walls with '|'
			if (j == 0 || j == width)
				cout << "|";
			// Creating snake's head with 'O'
			if (i == y && j == x)
				cout << "O";
			// Creating the snake's food with '#'
			else if (i == fruitCordY && j == fruitCordX)
				cout << "#";
			// Creating the slow fruit with '*'
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

	// Creating bottom walls with '-'
	for (int i = 0; i < width + 2; i++)
		cout << "-";
	cout << endl;

	// Display player's score and difficulty
	cout << playerName << "'s Score: " << playerScore;
	if (highScore > 0)
		cout << "  |  High Score: " << highScore << " (" << highScoreName << ")";
	cout << endl;

	// Display difficulty
	string diffLabel = (frameMs == 200) ? "Easy" : (frameMs == 150) ? "Normal" : "Hard";
	cout << "Difficulty: " << diffLabel << endl;

	if (slowEffectTicks > 0)
		cout << "** SLOW! (" << slowEffectTicks << " ticks 남음) **" << endl;
	cout << "Fruit: # = +10pts  |  * = +5pts + 속도 감소" << endl;

	// Display mode
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

// Function for updating the game state
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

	// Checks for collision with the tail (o)
	for (int i = 0; i < snakeTailLen; i++)
	{
		if (snakeTailX[i] == x && snakeTailY[i] == y)
			isGameOver = true;
	}

	// Checks for snake's collision with the food (#)
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
	}

	// Checks for snake's collision with slow fruit (*)
	if (slowFruitActive && x == slowFruitX && y == slowFruitY)
	{
		playerScore += 5;
		snakeTailLen++;
		UpdateFNDScore(playerScore);
		slowFruitActive = false;
		slowEffectTicks = SLOW_DURATION;
	}

	// Decrement slow effect
	if (slowEffectTicks > 0)
		slowEffectTicks--;
}

// Function to handle keyboard input
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

// Function to handle FPGA push switch input
void FPGASwitchInput()
{
	if (fd_fpga_switch < 0)
		return;

	unsigned char sw_state[13];
	if (read(fd_fpga_switch, sw_state, 13) > 0)
	{
		// Button mapping: 1=UP, 3=LEFT, 5=RIGHT, 7=DOWN
		if (sw_state[1])
			sDir = UP;
		if (sw_state[3])
			sDir = LEFT;
		if (sw_state[5])
			sDir = RIGHT;
		if (sw_state[7])
			sDir = DOWN;
	}
}

// Function to handle interrupt switch input
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

// Show a single-shot pause menu when the game is paused via interrupt.
// Returns true to resume, false to end the game (treat as game over).
bool PauseMenu()
{
	disableRawMode();

	cout << "\n=== Game Paused ===" << endl;
	if (use_fpga_switch)
	{
		cout << "FPGA: Button 0 = Resume, Button 2 = Exit" << endl;
	}
	if (!use_fpga_switch)
	{
		cout << "Keyboard: r = resume" << endl;
	}

	unsigned char prev_fpga[13] = {0};
	unsigned char prev_sw = 0;

	// If FPGA switches are available, handle ONLY Button 0(resume) and Button 2(exit).
	if (use_fpga_switch && fd_fpga_switch >= 0)
	{
		// consume current state and wait for release if pressed
		unsigned char sw_state[13];
		if (read(fd_fpga_switch, sw_state, 13) > 0)
		{
			memcpy(prev_fpga, sw_state, 13);
			bool anyPressed = (prev_fpga[0] || prev_fpga[2]);
			while (anyPressed)
			{
				if (read(fd_fpga_switch, sw_state, 13) > 0)
				{
					memcpy(prev_fpga, sw_state, 13);
					anyPressed = (prev_fpga[0] || prev_fpga[2]);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}

		while (true)
		{
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				// resume on rising edge of button 0
				if (sw_state[0] && !prev_fpga[0])
				{
					enableRawMode();
					return true;
				}
				// exit on rising edge of button 2
				if (sw_state[2] && !prev_fpga[2])
				{
					return false;
				}
				memcpy(prev_fpga, sw_state, 13);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	// If no FPGA push switch, fallback to keyboard only.
	while (true)
	{
		// Check FPGA push switch (if present)
		if (use_fpga_switch && fd_fpga_switch >= 0)
		{
			unsigned char sw_state[13];
			if (read(fd_fpga_switch, sw_state, 13) > 0)
			{
				if (sw_state[0] && !prev_fpga[0])
				{
					enableRawMode();
					return true;
				}
				if (sw_state[2] && !prev_fpga[2])
				{
					return false;
				}
				memcpy(prev_fpga, sw_state, 13);
			}
		}

		// Check keyboard input
		fd_set set;
		struct timeval tv;
		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		tv.tv_sec = 0;
		tv.tv_usec = 100000; // 100ms
		if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0)
		{
			int c = getchar();
			if (c == 'r' || c == 'R')
			{
				enableRawMode();
				return true;
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

// Main function / game looping function
int main()
{
	// Try to open FPGA push switch device
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

	// Try to open FPGA FND device
	fd_fpga_fnd = open("/dev/fpga_fnd", O_RDWR);
	if (fd_fpga_fnd >= 0)
	{
		cout << "FPGA FND device opened successfully!" << endl;
	}
	else
	{
		cout << "FPGA FND device not found. Score will be shown on console only." << endl;
	}

	// Try to open interrupt switch device
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

	GameInit();

	// Select difficulty and mode before starting
	unsigned int frameMs = SelectDifficulty();
	wrapWalls = SelectMode();
	enableRawMode();

	while (true)
	{
		while (!isGameOver)
		{
			unsigned int currentFrameMs = (slowEffectTicks > 0) ? frameMs * 2 : frameMs;
			GameRender("Player", frameMs);

			// Interrupt switch is always checked first so it can pause/resume the game
			if (use_interrupt_switch)
			{
				InterruptSwitchInput();

				// If paused by interrupt, show a single-shot pause menu (no repeated prints)
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

			// Handle movement input only when the game is not paused
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

		// Re-select difficulty and mode on restart
		frameMs = SelectDifficulty();
		wrapWalls = SelectMode();
		enableRawMode();
	}

	// Close devices if opened
	if (fd_fpga_switch >= 0)
		close(fd_fpga_switch);
	if (fd_fpga_fnd >= 0)
		close(fd_fpga_fnd);
	if (fd_sw >= 0)
		close(fd_sw);

	return 0;
}
