#include <stdio.h>
#include <getopt.h>

#define HELP_VERSION        1

static void help(const char *prog)
{
    printf("Usage:\n\t%s [option]\n\n", prog);
    printf("Why do you need help to get help?\n");
    printf("Use <command> --help if you want more information about a specific command;\n\n");
    printf("Options:\n");
    printf("\t-h, --help                Displays help;\n");
    printf("\t-V, --version             Displays version;\n");
    printf("\n");
}

static void about()
{
    printf("help for Foxdroid; Build %i\n", HELP_VERSION);
}

int help_main(int argc, char *argv[])
{
	int arg;

    static const struct option options[] = 
	{
		{ "version",    no_argument, NULL, 'V' },
		{ "help",       no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	while ((arg = getopt_long(argc, argv, "Vh", options, NULL)) != -1)
	{
		switch (arg)
		{
			case 'V':
				command_about();
				return 0;
			case 'h':
				command_help(argv[0]);
				return 0;
			default:
				break;
		}
	}

    printf("List of available commands:\n");

        #define TOOL(name) printf("  %s\n", #name);
            #include "tools.h"
        #undef TOOL

    return 0;
}
