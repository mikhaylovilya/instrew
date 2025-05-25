#include <common.h>
#include <logger.h>
static int logger_level = 0;

int logger_init(int mode)
{
	if (mode < 1)
		return 0;

	printf("logger mode = %u\n", mode);
	logger_level = mode;
	return 0;
}
