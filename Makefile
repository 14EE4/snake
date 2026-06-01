CC := arm-linux-gnueabi-gcc
CXX := arm-linux-gnueabi-g++

CFLAGS := -Wall -Wextra -O2
LDLIBS := -lm

.PHONY: all clean

all: snake buzzer_test note_name_test

snake: snake.cpp buzzer.c buzzer.h
	$(CXX) snake.cpp buzzer.c -o $@ -pthread $(LDLIBS)

buzzer_test: buzzer_test.c buzzer.c buzzer.h
	$(CC) $(CFLAGS) buzzer_test.c buzzer.c -o $@ $(LDLIBS)

note_name_test: note_name_test.c buzzer.c buzzer.h
	$(CC) $(CFLAGS) note_name_test.c buzzer.c -o $@ $(LDLIBS)

clean:
	rm -f snake buzzer_test note_name_test *.o