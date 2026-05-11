# Snake Game with FPGA Control

FPGA 버튼과 FND 디스플레이를 연동하는 snake 게임입니다.

## Features

- FPGA push switch로 게임 조작 (버튼: 1=UP, 3=LEFT, 5=RIGHT, 7=DOWN)
- 점수를 FND 디스플레이에 4자리 숫자로 표시
- 키보드 입력 지원 (fallback)

## Device Mapping

- `fpga_push_switch`: major `265`
- `fpga_fnd`: major `261`
- `my_led_dev`: major `236` (interrupt switch)

## FPGA Setup (라즈베리파이)

모듈 로드 및 디바이스 노드 생성:

```bash
# 1) 기본 인터페이스 드라이버 먼저 로드
sudo insmod fpga_interface_driver.ko

# 2) FPGA 디바이스 드라이버 로드
sudo insmod fpga_push_switch_driver.ko
sudo insmod fpga_fnd_driver.ko

# 3) 인터럽트 스위치 드라이버 로드
sudo insmod itr_driver.ko

# 4) FPGA 디바이스 노드 생성
sudo mknod /dev/fpga_push_switch c 265 0
sudo mknod /dev/fpga_fnd c 261 0
sudo mknod /dev/my_led_dev c 236 0

# 5) 권한 부여
sudo chmod 666 /dev/fpga_push_switch /dev/fpga_fnd /dev/my_led_dev

# 6) 모듈 로드 확인
lsmod | grep -E "fpga|itr"
dmesg | tail -20

# 7) 디바이스 노드 확인
ls -l /dev/fpga_push_switch /dev/fpga_fnd /dev/my_led_dev

# 정상이면 이렇게 나옵니다:
# crw-r--r-- 1 root root 265, 0 ... /dev/fpga_push_switch
# crw-r--r-- 1 root root 261, 0 ... /dev/fpga_fnd
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

## Cleanup

모듈 언로드 (역순):

```bash
sudo rmmod itr_driver
sudo rmmod fpga_fnd_driver
sudo rmmod fpga_push_switch_driver
sudo rmmod fpga_interface_driver
sudo rm -f /dev/fpga_push_switch /dev/fpga_fnd /dev/my_led_dev
```

## Control Methods (우선순위 순)

1. **FPGA Push Switch**: 버튼 1,3,5,7로 조작 (4개 버튼)
2. **Interrupt Switch**: GPIO2 기반 인터럽트로 게임 pause/resume 토글
3. **Keyboard**: 8,2,4,6 키로 조작 (fallback)

게임 시작 시 어떤 입력 방식이 활성화되었는지 화면에 표시됩니다.

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
