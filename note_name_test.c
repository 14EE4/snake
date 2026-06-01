#include <stdio.h>
#include <string.h>

#include "buzzer.h"

typedef struct
{
	const char *input;
	int expected;
} NoteCase;

typedef struct
{
	const char *input;
} InvalidCase;

static int run_valid_case(const NoteCase *test_case)
{
	int freq = 0;
	int result = note_name_to_frequency(test_case->input, &freq);
	if (result != 0)
	{
		fprintf(stderr, "FAIL: %s -> parse error\n", test_case->input);
		return 1;
	}

	if (freq != test_case->expected)
	{
		fprintf(stderr, "FAIL: %s -> got %d, expected %d\n", test_case->input, freq, test_case->expected);
		return 1;
	}

	printf("OK: %s -> %d\n", test_case->input, freq);
	return 0;
}

static int run_invalid_case(const InvalidCase *test_case)
{
	int freq = 0;
	int result = note_name_to_frequency(test_case->input, &freq);
	if (result == 0)
	{
		fprintf(stderr, "FAIL: %s -> expected parse failure, got %d\n", test_case->input, freq);
		return 1;
	}

	printf("OK: %s -> rejected\n", test_case->input);
	return 0;
}

int main(void)
{
	const NoteCase valid_cases[] = {
		{"A4", 440},
		{"  a4", 440},
		{"C4", 262},
		{"G#4", 415},
		{"Ab4", 415},
		{"Bb3", 233},
		{"C5", 523},
	};

	const InvalidCase invalid_cases[] = {
		{""},
		{"H4"},
		{"A"},
		{"C#"},
		{"Z9"},
	};

	int failures = 0;

	for (size_t i = 0; i < sizeof(valid_cases) / sizeof(valid_cases[0]); ++i)
	{
		failures += run_valid_case(&valid_cases[i]);
	}

	for (size_t i = 0; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i)
	{
		failures += run_invalid_case(&invalid_cases[i]);
	}

	if (failures == 0)
	{
		printf("All note parsing tests passed.\n");
		return 0;
	}

	printf("%d test(s) failed.\n", failures);
	return 1;
}