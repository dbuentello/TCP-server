//
// Command table for BREWise controller
//


static char temperature (void)
{
	return 88;
}

char Interpreter (char* string)
{

	const struct CommandTable
	{
		char* command;   // type this letter
		char (*fnctPt)(void);   // to execute this command
	};
	typedef const struct CommandTable CommandTableType;

	int NumCmds = 1;

	CommandTableType CT[1]={ //Command Table
	{ "temperature\0", &temperature}, //Move Forward //:00MF1234F7
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
		else
		{
			return (-1);
		}

	}

	return 0;
}
