#include "Helpers.h"

#include <errno.h>
#include <stdlib.h> // strtol
#include <limits.h> // LONG_MIN

bool ParseUnsignedInt(const char* arg, unsigned int& val)
{
	errno = 0;
	char *pEnd = NULL;
	long int iVal = strtol(arg, &pEnd, 0);
	if(iVal < 0 || arg == pEnd || errno != 0 || iVal == LONG_MIN || iVal == LONG_MAX)
	{
		return false;
	}
	val = (unsigned int)iVal;
	return true;
}

bool ParseFloat(const char* arg, float& val)
{
	errno = 0;
	char *pEnd = NULL;
	float fVal = strtof(arg, &pEnd);
	if(errno != 0)
	{
		return false;
	}
	val = fVal;
	return true;
}
