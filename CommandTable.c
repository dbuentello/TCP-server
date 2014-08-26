//
// Command table for BREWise controller
//
#include <stdio.h>
#include <stdlib.h>
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "pin.h"
#include "gpio.h"
#include "gpio_if.h"

static char orangeStatus = 0;
static char *arguments[3];
static int setTemp = 0;

static char setTemperature (char argCountL, char **argumentsL)
{
	setTemp = atoi(arguments[0]);
	return 0;
}

static char getTemperature (char argCountL, char **argumentsL)
{
	return setTemp;
}

static char orange (char argCountL, char **argumentsL)
{
	if(orangeStatus == 0)
	{
		GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
		orangeStatus = 1;
	}
	else
	{
		GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);
		orangeStatus = 0;
	}
	return 0;
}

char Interpreter (char* string)
{

	const struct CommandTable
	{
		char* command;   // type this letter
		char (*fnctPt)(char, char**);   // to execute this command
	};
	typedef const struct CommandTable CommandTableType;

	int NumCmds = 3;

	CommandTableType CT[3]={ //Command Table
	{"setTemperature\0", &setTemperature}, // Sets the setTemp variable with first argument
	{"getTemperature\0", &getTemperature},
	{"orange\0", &orange},				// Turns orange light on or off
	};

	int i = 0;
	int j = 0;
	char argCount = 0;
	char foundArgument = 0;
	char *stringSearch;

	//
	// Parse the string for arguments
	//
	stringSearch = string;

	while(*stringSearch)
	{
		if(*stringSearch == ' ')
		{
			foundArgument = 1;
			*stringSearch = 0;
		}
		else
		{
			if(foundArgument && (argCount < 2))
			{
				arguments[argCount] = stringSearch;
				argCount++;
				foundArgument = 0;
			}
		}
		stringSearch++;
	}

	//
	// Find match for command
	//
	for(j = 0; j < NumCmds; j++)
	{
		for(i = 0; string[i] == CT[j].command[i]; i++)
		{
			if((string[i] == '\0') || (CT[j].command[i] == '\0'))
			{
				break;
			}
		}

		if((string[i] == '\0') && (CT[j].command[i] == '\0'))
		{
			return CT[j].fnctPt(argCount, arguments);
		}

	}

	return -1;
}
