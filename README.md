# Snake Game with FPGA Control

FPGA 버튼과 FND 디스플레이를 연동하는 snake 게임입니다.

## Features

- FPGA push switch로 게임 조작 (버튼: 1=UP, 3=LEFT, 5=RIGHT, 7=DOWN)
- 점수를 FND 디스플레이에 4자리 숫자로 표시
- 키보드 입력 지원 (fallback)

## Device Mapping

- `fpga_push_switch`: major `265`
- `fpga_fnd`: major `261`

## FPGA Setup (라즈베리파이)

모듈 로드 및 디바이스 노드 생성:

```bash
# 1) 기본 인터페이스 드라이버 먼저 로드
sudo insmod fpga_interface_driver.ko

# 2) FPGA 디바이스 드라이버 로드
sudo insmod fpga_push_switch_driver.ko
sudo insmod fpga_fnd_driver.ko

# 3) 디바이스 노드 생성
sudo mknod /dev/fpga_push_switch c 265 0
sudo mknod /dev/fpga_fnd c 261 0

# 4) 권한 부여
sudo chmod 666 /dev/fpga_push_switch /dev/fpga_fnd

# 5) 확인
ls -l /dev/fpga_push_switch /dev/fpga_fnd

# 정상이면 이렇게 나옵니다:
# crw-r--r-- 1 root root 265, 0 ... /dev/fpga_push_switch
# crw-r--r-- 1 root root 261, 0 ... /dev/fpga_fnd
```

## Build (Windows/WSL)

```bash
cd ~/workspace/snake
arm-linux-gnueabi-g++ snake.cpp -o snake -pthread
```

## Deploy & Run (라즈베리파이)

```bash
# 1) 바이너리 전송 (Windows/WSL에서)
scp -P 51234 snake pi02@210.125.213.129:/home/pi02/workspace/

# 2) 라즈베리파이에서 실행
ssh -p 51234 pi02@210.125.213.129
cd ~/workspace
sudo ./snake
```

## Cleanup

모듈 언로드 (역순):

```bash
sudo rmmod fpga_fnd_driver
sudo rmmod fpga_push_switch_driver
sudo rmmod fpga_interface_driver
sudo rm -f /dev/fpga_push_switch /dev/fpga_fnd
```
