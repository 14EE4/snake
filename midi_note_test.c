#include <stdio.h>
#include "buzzer.h"

static int expect_frequency(int midi_note, int expected)
{
	int freq = 0;
	int result = midi_note_to_frequency(midi_note, &freq);
	if (result != 0)
	{
		printf("midi_note_to_frequency(%d) failed\n", midi_note);
		return 1;
	}
	if (freq != expected)
	{
		printf("midi_note_to_frequency(%d) = %d, expected %d\n", midi_note, freq, expected);
		return 1;
	}
	return 0;
}

int main(void)
{
	if (expect_frequency(60, 262) != 0) return 1; // C4
	if (expect_frequency(69, 440) != 0) return 1; // A4
	if (expect_frequency(72, 523) != 0) return 1; // C5

	int freq = 0;
	if (midi_note_to_frequency(0, &freq) == 0)
	{
		printf("Expected MIDI note 0 to be rejected\n");
		return 1;
	}

	printf("All MIDI note tests passed.\n");
	return 0;
}