# Snake Game with FPGA Control

FPGA 버튼과 FND 디스플레이를 연동하는 snake 게임입니다.

## Features

- FPGA push switch로 게임 조작 (버튼: 1=UP, 3=LEFT, 5=RIGHT, 7=DOWN)
- 난이도 선택은 0=Easy, 1=Normal, 2=Hard로 입력
- 점수를 FND 디스플레이에 4자리 숫자로 표시
- 키보드 입력 지원 (fallback)
- 벽 또는 자기 꼬리 충돌 시 게임 오버 처리
 - 시작 시 모드 선택 추가: `Normal` (벽 닿으면 죽음) 또는 `Wrap` (벽 닿으면 반대편으로 넘어감). FPGA 버튼 또는 키보드로 선택 가능.
- 게임 오버 후 FPGA Switch 0으로 재시작, 2로 종료 선택 가능
	- (확인) 일시정지 상태에서 FPGA 버튼 0으로 재개하고 버튼 2로 종료할 수 있도록 기능을 추가했으며, 스위치 동작을 확인했습니다.
- 매 게임마다 이름 입력받고 점수 기록
- 최고 점수 및 기록자 이름 추적
- 새로운 기록 달성 시 축하 메시지 표시
- 최고 점수 미달 시 현재 점수와 최고 점수, 각각의 플레이어 이름 표시

## Device Mapping

- `fpga_push_switch`: major `265`
- `fpga_fnd`: major `261`
- `my_led_dev`: major `236` (interrupt switch)
- `fpga_buzzer`: major `264`

## Buzzer Test Example

버저 드라이버는 1바이트 write/read 방식입니다. 포함된 테스트 유틸리티 `buzzer_test` 로 여러 동작을 확인할 수 있습니다.

빌드 및 준비

```bash
# (라즈베리파이에서) 모듈을 삽입한 뒤 장치 노드 생성
sudo insmod fpga_buzzer_driver.ko
sudo mknod /dev/fpga_buzzer c 264 0
sudo chmod 666 /dev/fpga_buzzer

# 빌드 (로컬이나 라즈베리파이에서)
gcc buzzer_test.c -o buzzer_test -lm
```

기본 사용법

```bash
./buzzer_test on               # 짧게 켰다가 끕니다 (기본 200ms)
./buzzer_test off              # 끕니다
./buzzer_test pulse            # 짧은 펄스 (기본 약 150ms)
./buzzer_test blink <n> <ms>   # n회 깜빡임, 각 on/off 지연(ms)
./buzzer_test tone <hz> <ms>   # 지정 주파수(hz)를 duration(ms)만큼 재생
./buzzer_test note <name> <ms> # 음계 이름(C4, A3, G#5 등)을 duration(ms)만큼 재생
./buzzer_test test             # C major 스케일(C4..C5)을 연주
```

예시

```bash
./buzzer_test tone 440 500    # 440Hz를 500ms 재생 (A4)
./buzzer_test note C4 300     # C4(약 261Hz)를 300ms 재생
./buzzer_test blink 5 200     # 5회 200ms 간격으로 on/off
./buzzer_test test            # 도레미파솔라시도 재생
```

설명 및 제약

- `tone` 모드는 직접 주파수를 받습니다. 유저스페이스 타이머 정밀도의 한계로 매우 높은 주파수나 정밀 주파수 제어에는 한계가 있습니다.
- `note` 모드는 `A4 = 440Hz` 기준으로 음계 이름을 계산합니다. 지원 예: `C4`, `G#4` (또는 `Ab4`), `A3` 등. 옥타브 숫자를 반드시 포함해야 합니다.
- 주파수 범위: 내부에서 20Hz ~ 20000Hz 로 제한합니다.
- 볼륨(실제 음량) 제어는 하드웨어(버저 회로)나 FPGA 쪽 PWM/증폭기 설계가 필요합니다. 유저스페이스에서는 듀티비 조작으로 감지상 음량을 낮출 수 있으나 효과는 하드웨어에 따라 다릅니다.

빠른 확인: 장치를 올바르게 로드하면 `ls -l /dev/fpga_buzzer` 로 존재를 확인하고, `dmesg`에 관련 로그가 남을 수 있습니다.


## FPGA Setup (라즈베리파이)

모듈 로드 및 디바이스 노드 생성:

```bash
# 1) 기본 인터페이스 드라이버 먼저 로드
sudo insmod fpga_interface_driver.ko

# 2) FPGA 디바이스 드라이버 로드
sudo insmod fpga_push_switch_driver.ko
sudo insmod fpga_fnd_driver.ko
sudo insmod fpga_buzzer_driver.ko

# 3) 인터럽트 스위치 드라이버 로드
sudo insmod itr_driver.ko

# 4) FPGA 디바이스 노드 생성
sudo mknod /dev/fpga_push_switch c 265 0
sudo mknod /dev/fpga_fnd c 261 0
sudo mknod /dev/fpga_buzzer c 264 0
sudo mknod /dev/my_led_dev c 236 0

# 5) 권한 부여
sudo chmod 666 /dev/fpga_push_switch /dev/fpga_fnd /dev/fpga_buzzer /dev/my_led_dev

# 6) 모듈 로드 확인
lsmod | grep -E "fpga|itr"
dmesg | tail -20

# 7) 디바이스 노드 확인
ls -l /dev/fpga_push_switch /dev/fpga_fnd /dev/fpga_buzzer /dev/my_led_dev

# 정상이면 이렇게 나옵니다:
# crw-r--r-- 1 root root 265, 0 ... /dev/fpga_push_switch
# crw-r--r-- 1 root root 261, 0 ... /dev/fpga_fnd
# crw-r--r-- 1 root root 264, 0 ... /dev/fpga_buzzer
# crw-r--r-- 1 root root 236, 0 ... /dev/my_led_dev
```

## Build (Windows/WSL)

```bash
cd ~/workspace/snake
arm-linux-gnueabi-g++ snake.cpp -o snake -pthread
```

## Deploy & Run (라즈베리파이)

```bash
# 1) 라즈베리파이 접속
ssh pi02@210.125.213.129 -p 51234

# 2) 바이너리 전송 (Windows/WSL에서)
scp -P 51234 snake pi02@210.125.213.129:/home/pi02/workspace/

# 3) 라즈베리파이에서 실행
ssh -p 51234 pi02@210.125.213.129
cd ~/workspace
sudo ./snake
```

## Example Run

```text
FPGA push switch device opened successfully!
FPGA FND device opened successfully!
Interrupt switch device opened successfully!

----------------------------------------------------------------------------------
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                                                |
|                                                         #                      |
|                                                                                |
|                                                                                |
|                        o                                                       |
|                        O                                                       |
----------------------------------------------------------------------------------
Player's Score: 10
Control: Button 1=Up / 3=Left / 5=Right / 7=Down (FPGA Switch)
Enter your name to record this score: John
Game Over! Final Score: 10
*** NEW HIGH SCORE! ***

Restart? FPGA Switch 0 = restart, 2 = exit

(... 다시 게임 진행 후 ...)

Player's Score: 5
Game Over! Final Score: 5
Enter your name to record this score: Alice
Current Score: 5 (Alice)
High Score: 10 (John)

Restart? FPGA Switch 0 = restart, 2 = exit
```

## Cleanup

모듈 언로드 (역순):

```bash
sudo rmmod itr_driver
sudo rmmod fpga_buzzer_driver
sudo rmmod fpga_fnd_driver
sudo rmmod fpga_push_switch_driver
sudo rmmod fpga_interface_driver
sudo rm -f /dev/fpga_push_switch /dev/fpga_fnd /dev/fpga_buzzer /dev/my_led_dev
```

## Control Methods (우선순위 순)

1. **FPGA Push Switch**: 버튼 1,3,5,7로 조작 (4개 버튼)
2. **Interrupt Switch**: GPIO2 기반 인터럽트로 게임 pause/resume 토글
3. **Keyboard**: 8,2,4,6 키로 조작 (fallback)

난이도 선택은 FPGA 스위치 0,1,2 또는 키보드 0,1,2로 진행합니다.

게임 시작 시 어떤 입력 방식이 활성화되었는지 화면에 표시됩니다.

## Mode Selection

- 게임 시작 시 모드를 선택합니다:
	- `Normal`: 벽에 닿으면 게임 오버 (기존 동작)
	- `Wrap`: 벽을 통과하여 반대편으로 이동 (새로운 옵션)

- 선택 방법:
	- FPGA가 연결된 경우: 버튼 0 = `Normal`, 버튼 1 = `Wrap`.
	- FPGA 미연결(키보드 사용)인 경우: `0` = `Normal`, `1` = `Wrap` 입력.

- 선택한 모드는 게임 화면에 `Mode:` 항목으로 표시됩니다.

## Restart Behavior

- 게임 시작 시 이름 입력을 받지 않고 바로 시작됩니다.
- 게임 오버가 되면 점수를 기록할 이름을 입력받습니다.
- 현재 점수가 최고 점수보다 높으면 최고 점수로 갱신되고 "*** NEW HIGH SCORE! ***" 메시지를 표시합니다.
- 현재 점수가 최고 점수보다 낮으면 현재 점수 및 이름, 그리고 최고 점수 및 그 기록자의 이름을 함께 표시합니다.
- 이후 FPGA Switch 0으로 재시작하거나, 2로 종료를 선택합니다.
- FPGA Switch가 없을 때는 기존처럼 `y/n` 키보드 입력으로 재시작을 선택합니다.
- 최고 점수는 게임 실행 중 메모리에 유지되며, 현재 세션 동안 갱신됩니다.

## Interrupt Switch Behavior

- `my_led_dev`는 GPIO2의 rising edge 인터럽트로 동작합니다.
- 스위치를 한 번 누르면 게임이 일시정지(pause)됩니다.
- 다시 인터럽트가 들어오면 게임이 이어서 진행(resume)됩니다.
- 입력 우선순위는 `FPGA Push Switch` > `Interrupt Switch` > `Keyboard` 순서입니다.

## Interrupt Switch Device

- Device file: `/dev/my_led_dev`
- Major number: `236`
- GPIO input: `27`
- GPIO output: `24`
- Interrupt trigger: `IRQF_TRIGGER_RISING`
