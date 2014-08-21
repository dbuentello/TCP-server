//
// Command table for BREWise controller
//
#include <stdio.h>
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "pin.h"
#include "gpio.h"
#include "gpio_if.h"

static char orangeStatus = 0;
static char BWLED2Pin;

static char temperature (void)
{
	return 88;
}

static char orange (void)
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
		char (*fnctPt)(void);   // to execute this command
	};
	typedef const struct CommandTable CommandTableType;

	int NumCmds = 2;

	CommandTableType CT[2]={ //Command Table
	{"temperature\0", &temperature}, //Move Forward //:00MF1234F7
	{"orange\0", &orange},				// Turns orange light on or off
	};

	int i = 0;
	int j = 0;

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
			return CT[j].fnctPt();
		}

	}

	return -1;
}
