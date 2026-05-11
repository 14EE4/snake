// required header file 
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

using namespace std;

// height and width of the boundary 
const int width = 80; 
const int height = 20;

// FPGA push switch device
int fd_fpga_switch = -1;
bool use_fpga_switch = false; 

// Snake head coordinates of snake (x-axis, y-axis) 
int x, y; 
// Food coordinates 
int fruitCordX, fruitCordY; 
// variable to store the score of he player 
int playerScore; 
// Array to store the coordinates of snake tail (x-axis, 
// y-axis) 
int snakeTailX[100], snakeTailY[100]; 
// variable to store the length of the sanke's tail 
int snakeTailLen; 
// for storing snake's moving snakesDirection 
enum snakesDirection { STOP = 0, LEFT, RIGHT, UP, DOWN }; 
// snakesDirection variable 
snakesDirection sDir; 
// boolean variable for checking game is over or not 
bool isGameOver; 

// Function to initialize game variables 
void GameInit() 
{ 
	isGameOver = false; 
	sDir = STOP; 
	x = width / 2; 
	y = height / 2; 
	fruitCordX = rand() % width; 
	fruitCordY = rand() % height; 
	playerScore = 0; 
} 

// Function for creating the game board & rendering 
void GameRender(string playerName) 
{ 
	#ifdef _WIN32
	std::system("cls"); // Clear the console (Windows)
	#else
	std::system("clear"); // Clear the console (POSIX)
	#endif

	// Creating top walls with '-' 
	for (int i = 0; i < width + 2; i++) 
		cout << "-"; 
	cout << endl; 

	for (int i = 0; i < height; i++) { 
		for (int j = 0; j <= width; j++) { 
			// Creating side walls with '|' 
			if (j == 0 || j == width) 
				cout << "|"; 
			// Creating snake's head with 'O' 
			if (i == y && j == x) 
				cout << "O"; 
			// Creating the sanke's food with '#' 
			else if (i == fruitCordY && j == fruitCordX) 
				cout << "#"; 
			// Creating snake's head with 'O' 
			else { 
				bool prTail = false; 
				for (int k = 0; k < snakeTailLen; k++) { 
					if (snakeTailX[k] == j 
							&& snakeTailY[k] == i) { 
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

	// Display player's score 
	cout << playerName << "'s Score: " << playerScore << endl; 
	if (use_fpga_switch)
		cout << "Control: Button 3=Left / 5=Right / 7=Up / 1=Down" << endl;
	else
		cout << "Control: Up: 8 / Down: 2 / Left: 4 / Right: 6" << endl;
} 

// Function for updating the game state 
void UpdateGame() 
{ 
	int headX = x;
	int headY = y;
	int prevX = snakeTailX[0]; 
	int prevY = snakeTailY[0]; 
	int prev2X, prev2Y;

	switch (sDir) { 
		case LEFT: 
			x--;
			if (x < 0) {x = 0;	return;}
			break; 
		case RIGHT: 
			x++; 
			if (x >= width) {x = width-1; return;}
			break; 
		case UP: 
			y--; 
			if (y < 0) {y = 0; return;}
			break; 
		case DOWN: 
			y++; 
			if (y >= height) {y = height-1; return;}
			break; 
	} 

	snakeTailX[0] = headX; 
	snakeTailY[0] = headY; 

	for (int i = 1; i < snakeTailLen; i++) { 
		prev2X = snakeTailX[i]; 
		prev2Y = snakeTailY[i]; 
		snakeTailX[i] = prevX; 
		snakeTailY[i] = prevY; 
		prevX = prev2X; 
		prevY = prev2Y; 
	} 

	// Checks for collision with the tail (o) 
	for (int i = 0; i < snakeTailLen; i++) { 
		if (snakeTailX[i] == x && snakeTailY[i] == y) 
			isGameOver = true; 
	} 

	// Checks for snake's collision with the food (#) 
	if (x == fruitCordX && y == fruitCordY) { 
		playerScore += 10; 
		fruitCordX = rand() % width; 
		fruitCordY = rand() % height; 
		snakeTailLen++; 
	} 
} 

// Function to handle user UserInput 
void UserInput() 
{ 
	// legacy handler: uses platform-specific kbhit/getch wrappers
#ifdef _WIN32
	if (_kbhit()) {
		int key = _getch();
		switch (key) {
			case '4': sDir = LEFT; break;
			case '6': sDir = RIGHT; break;
			case '8': sDir = UP; break;
			case '2': sDir = DOWN; break;
			case '0': isGameOver = true; break;
		}
	}
#else
	fd_set set;
	struct timeval tv;
	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, &set);
	tv.tv_sec = 0; tv.tv_usec = 0;
	if (select(STDIN_FILENO+1, &set, NULL, NULL, &tv) > 0) {
		int key = getchar();
		switch (key) {
			case '4': sDir = LEFT; break;
			case '6': sDir = RIGHT; break;
			case '8': sDir = UP; break;
			case '2': sDir = DOWN; break;
			case '0': isGameOver = true; break;
		}
	}
#endif
}

// Function to handle FPGA push switch input
void FPGASwitchInput()
{
	if (fd_fpga_switch < 0) return;
	
	unsigned char sw_state[13];
	if (read(fd_fpga_switch, sw_state, 13) > 0) {
		// Button mapping: 1=UP, 3=LEFT, 5=RIGHT, 7=DOWN (위아래 반전)
		if (sw_state[1]) sDir = UP;        // Button 1
		if (sw_state[3]) sDir = LEFT;      // Button 3
		if (sw_state[5]) sDir = RIGHT;     // Button 5
		if (sw_state[7]) sDir = DOWN;      // Button 7
	}
} 
#ifndef _WIN32
static struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // non-canonical, no-echo
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    atexit(disableRawMode);
}
#endif

// Main function / game looping function 
int main() 
{ 
	string playerName; 
	cout << "enter your name: "; 
	cin >> playerName;

	// Try to open FPGA push switch device
	fd_fpga_switch = open("/dev/fpga_push_switch", O_RDWR);
	if (fd_fpga_switch >= 0) {
		use_fpga_switch = true;
		cout << "FPGA push switch device opened successfully!" << endl;
	} else {
		cout << "FPGA push switch device not found. Using keyboard input." << endl;
	}

	GameInit();
#ifndef _WIN32
enableRawMode();
#endif
	// Main game loop: render -> handle non-blocking input -> update -> wait
	const unsigned int frameMs = 150; // milliseconds per movement update
	while (!isGameOver) {
		GameRender(playerName);

		// Handle input from FPGA switch or keyboard
		if (use_fpga_switch) {
			FPGASwitchInput();
		} else {
			// platform-specific immediate-key handling
#ifdef _WIN32
			if (_kbhit()) {
				int key = _getch();
				switch (key) {
					case '4': sDir = LEFT; break;
					case '6': sDir = RIGHT; break;
					case '8': sDir = UP; break;
					case '2': sDir = DOWN; break;
					case '0': isGameOver = true; break;
				}
			}
#else
			fd_set set;
			struct timeval tv;
			FD_ZERO(&set);
			FD_SET(STDIN_FILENO, &set);
			tv.tv_sec = 0; tv.tv_usec = 0;
			if (select(STDIN_FILENO+1, &set, NULL, NULL, &tv) > 0) {
				int key = getchar();
				switch (key) {
					case '4': sDir = LEFT; break;
					case '6': sDir = RIGHT; break;
					case '8': sDir = UP; break;
					case '2': sDir = DOWN; break;
					case '0': isGameOver = true; break;
				}
			}
#endif
		}

		UpdateGame();
		std::this_thread::sleep_for(std::chrono::milliseconds(frameMs));
	}

	// Close FPGA device if opened
	if (fd_fpga_switch >= 0) {
		close(fd_fpga_switch);
	}

	return 0; 
}



