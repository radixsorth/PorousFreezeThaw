/***********************************************\
* CParse: The command and option parser		*
* (C) 2005, 2015 Pavel Strachota                *
* file: cparser.c                               *
\***********************************************/

/*
This is a set of tools inspired by the Linux 'getopt' library,
designed to handle command lines (in fact, also strings on multiple lines) in the format

command option1[=value] option2[=value] ...

Any number of whitespaces can be placed:
- before the command keyword
- between options
- between an option and the '=' character
- between the '=' character and the option value

RULES: Everything on the line after the # character is treated as a comment. The
names of commands and their options can consist of all non-whitespace characters
except of '#' and '='. The option value starts at the first non-whitespace
character after '=' and ends before the first whitespace after that character.
The '=' character at the very end of the line is ignored. Alternatively, you can
include a quotation mark " in the value It will be ignored, but all characters
including spaces and '#' will be treated as value data up to the next quotation mark.
If you need to include the " character in the value, you must escape it, that is,
write \" . You can also supply an empty value by typing my_option="".

The maximum length of a single word/value is CPARSER_STRING_LIMIT characters.

HOW IT WORKS: there are several functions that deal with commands. The most complex
version CP_runcommand() parses the line and calls handlers for each option and
for the command itself. The handlers are defined through the CP_COMMAND and
CP_OPTION structures. Note that this toolbox naturally allows multiple occurence
of the same option on the line. It is left up to you if you want to disable this
feature.

NOTE: For more info on how to construct the command structures, see cparser.h
*/

#include "cparser.h"

#define CPARSER_STRING_LIMIT 4095

CP_CURRENT_COMMAND CP_getcommand(_conststring_ line, CP_COMMAND * known_commands, FILE * verbose)
/*
scans the given line of text for the known commands. Returns a structure with
command information if a command is found. Returns a special object which has
cmd_index==-1 if the line contains no commands (empty line or remark). Returns
an object which has cmd_index==-2 on error (command does not exist). Does NOT
call any command handling functions. If the stream 'verbose' is not NULL,
CP_getcommand() will output error messages to this stream.
*/
{
	CP_CURRENT_COMMAND cc={-2, 0, NULL, NULL, NULL};
	char cmd[CPARSER_STRING_LIMIT+1];
	_string_ cm=cmd;
	int i;

	/* skip leading whitespaces */
	while(*line==' ' || *line==0x9 || *line=='\n') line++;

	for(i=0;i<CPARSER_STRING_LIMIT;i++) {	/* limit the command size to CPARSER_STRING_LIMIT bytes */
		switch(*line) {
			case '=':
				if(verbose) fprintf(verbose,"Error: Invalid command\n");
				return(cc); /* the command must not contain an '=' */
			case ' ':
			case 0x9:
			case '\n':	/* allow multiple lines */
			case 0:
			case '#': i=CPARSER_STRING_LIMIT; break;
			default: *(cm++)=*(line++);
		}
	}

	*cm=0;

	if(*cmd==0) { cc.cmd_index++; return cc; }	/* empty command */

	i=0;
	while(known_commands->name != NULL) {
		if(match(known_commands->name,cmd)) {
				cc.cmd_index=i;
				cc.cmd=known_commands;
				cc.opt_start=cc.position=line;
				return(cc);
		}
		known_commands++; i++;
	}

	/* command not found */
	/* (this message might not be always wanted:) */
	/* if(verbose) fprintf(verbose,"Error: Invalid command '%s'.\n",cmd); */
	return(cc);
}

int CP_walk_opt(CP_CURRENT_COMMAND * cc, _string_ opt_value,FILE * verbose)
/*
walks through the options of the current command found by CP_getcommand. Returns
the subscript of the next available option in the option array associated to the
current command. (The i-th call returns the i-th option) If the option has a
value, the value string is copied into the location pointed to by opt_value.
There must be enough room available for that. opt_value will contain an empty
string if no value is given for the option that CAN have a value. opt_value
won't be modified for an option that can not have a value. You can force an
empty value even for an option that requires it by writing ' my_option="" '.

If the stream  'verbose' is not NULL, CP_walk_opt() will output error messages
to this stream.

CP_walk_opt does NOT call any option handling functions.

IMPORTANT: It is necessary to keep the whole command line unchanged in memory
while you are walking through commands!

return codes:
index	success
-1	invalid option
-2	option needs a value
-3	option cannot have a value
-4	no more options

For the last two return values, CP_walk_opt stores the subscript of the option
that caused the error in cc->val_error. If an error occurs, CP_walk_opt() will
not advance to the next option (successive calls to CP_walk_opt will report the
same error).
*/
{
	char option[CPARSER_STRING_LIMIT+1];
	char value[CPARSER_STRING_LIMIT+1]="";	/* this means that *value==0 */
	_string_ opt=option;
	_string_ vl=value;
	_conststring_ line=cc->position;
	CP_OPTION * options=cc->cmd->options;
	int i,j;

	int quotes=0;

	if(options==NULL) return(-4);	/* do not scan for options */

	/* skip leading whitespaces */
	while(*line==' ' || *line==0x9 || *line=='\n') line++;

	for(i=0;i<CPARSER_STRING_LIMIT;i++) {	/* limit the option size to CPARSER_STRING_LIMIT bytes */
		switch(*line) {
			case '=':
			case ' ':
			case 0x9:
			case '\n':	/* allow multiple lines */
			case 0:
			case '#': i=CPARSER_STRING_LIMIT; break;
			default: *(opt++)=*(line++);
		}
	}

	*opt=0;

	if(*option==0) {	/* no more options */
		if(*line=='=') {	/* assignment to no option */
			if(verbose) fprintf(verbose,"CParse error: Assignment to no option.\n");
			return(-1);
		}
		cc->position=line;	/* <-- do this so that subsequent calls finish immediately */
		return(-4);
	}

	i=0;
	while(options->name != NULL) {
		if(match(options->name,option)) {	/* a valid option found */

			/*
			check for option value NOW
			--------------------------
			*/

			/* skip whitespaces (there may be none) */
			while(*line==' ' || *line==0x9 || *line=='\n') line++;

			if(*line=='=') {	/* a value will (possibly) follow */

				/* skip whitespaces (there may be none) */
				line++;
				while(*line==' ' || *line==0x9 || *line=='\n') line++;

				/* read value data (may result in an empty string) */
				for(j=0;j<CPARSER_STRING_LIMIT;) {	/* limit the value size to CPARSER_STRING_LIMIT bytes */
					switch(*line) {
						case '"': /* allow use of quotes: do not consider whitespaces */
							line++;
							if(quotes) { j=CPARSER_STRING_LIMIT; break; }	/* closing quote */
							else { quotes=1; break; }			/* opening quote */

						/* these characters terminate reading if they are not in quotes */
						case ' ':
						case 0x9:
						case '\n':	/* allow multiple lines */
						case '#':
							if(quotes) { *(vl++)=*(line++); j++; break; } /* allow everything in quotes */

						/* end of line always terminates reading */
						case 0: j=CPARSER_STRING_LIMIT; break;

						/* backslash escapes the quote character '"' (even if not already in quotes)*/
						case '\\':
							if(*(line+1)=='"') line++;
							/* now, the '"' character will be read normally */

						default: *(vl++)=*(line++); j++;
					}
				}

				*vl=0;
			}

			/*
			at this point, either the value is read or is empty, which may
			have three reasons:
				1) there was a '=' but nothing more followed
				2) there was no '=' until the next non-whitespace character
				   or until the end of line.

			In the case when the value is empty because of (1) and (2), 'line' points to the first
			non-whitespace character that follows the option keyword. In the case when the value
			has been read, 'line' points to the first character after the value. ( However, both cases
			ensure correct processing by the next call to CP_walk_opt(). )

			An empty value will be treated as NO OPTION only if it was not written as "", that is,
			if quotes==0.
			*/

			if(*value==0 && quotes==0)
				switch(options->uses_value) {
					case CP_REQUIRED:
						cc->val_error=i;
						if(verbose) fprintf(verbose,"CParse error: Option '%s' for command '%s' requires a value.\n",option,cc->cmd->name);
						return(-2);
					case CP_OPTIONAL:
						set(opt_value,value);
					case CP_NONE:
						cc->position=line;
						return(i);
				}
			else
				switch(options->uses_value) {
					case CP_NONE:
						cc->val_error=i;
						if(verbose) fprintf(verbose,"CParse error: Option '%s' for command '%s' does not expect a value.\n",option,cc->cmd->name);
						return(-3);
					default:
						set(opt_value,value);
						cc->position=line;
						return(i);
				}
			/*
			check for option value ENDS
			---------------------------
			*/

		}
		options++; i++;
	}
	if(verbose) fprintf(verbose,"CParse error: Invalid option '%s' for command '%s'.\n",option,cc->cmd->name);
	return(-1);		/* invalid option */
}

void CP_restart_walk(CP_CURRENT_COMMAND * cc)
/*
rewinds the option pointer of the current command so that the next call to
CP_walk_opt returns the first command option.
*/
{
	cc->position=cc->opt_start;
}

int CP_handle_options(CP_CURRENT_COMMAND * cc, FILE * verbose)
/*
walks through all the options of the current command and calls their handlers if
they are not specified as NULL. If the stream 'verbose' is not NULL,
CP_handle_options() will output error messages to this stream.

The handler must take three arguments:
--------------------------------------
1) int :		the index of the current command in the command array
2) int :		the subscript of the option in the option array associated
			to the current command
3) _conststring_ :	pointer to the location where the option value is stored
			or a NULL pointer if the option does not have a value.

The handler must return one of the values defined in enum CP_STAT.

return codes:
0	success
-1	an invalid option encountered (here it means that CP_walk_opt() returned
	any error)
-2	the option handler returned CP_ERROR (processing has been interrupted)
-3	invalid current command (cc==NULL or cc='empty')
*/
{
	int i,j;
	char value[CPARSER_STRING_LIMIT];

	/* invalid current command structure */
	if(cc==NULL || cc->cmd_index<0) {
		if(verbose) fprintf(verbose,"CParse error: Void command does not have options.\n");
		return(-3);
	}

	while((i=CP_walk_opt(cc,value,verbose))>=0) {
		/* call the option handler if it exists */
		if(cc->cmd->options[i].handler != NULL) {
			j=cc->cmd->options[i].handler(cc->cmd_index,i,value);
			if(j==CP_ERROR) {
				if(verbose) fprintf(verbose,"CParse error: Handler for option '%s' failed.\n",cc->cmd->options[i].name);
				return(-2);
			}
		}
	}

	switch(i) {		/* error messages will be produced by CP_walk_opt() */
		case -4:	return(0);	/* no more options */
		case -1:
		case -2:
		case -3:	return(-1);
	}
	return(0);		/* just to be sure... */
}

int CP_runcommand(_conststring_ line, CP_COMMAND * known_commands, FILE * verbose)
/*
scans a line for a defined command, walks through its options and executes
all relevant handlers. The processing can be summarized as follows:

1) search for a command. Exit if none was found.
2) Run the preprocessing handler
3) parse all command options and call the option handlers
4) call the command handler (should perform the actual command as all information)

The preprocessing handler takes two arguments:
----------------------------------------------
1) CP_CURRENT_COMMAND * :	the pointer to the structure returned by
				CP_getcommand(). It can be used for custom
				option handling via the CP_walk_opt() function
				or to retrieve the command information through
				its members.
2) _conststring_ :		the pointer to the full option string.

The handler must return the pointer to a new option string, which will be
then used for option parsing. Return an empty string if you want to skip
automatic option parsing. Return NULL in case of an error.
-----
The main handler takes one argument: the subscript of the current command in the
known commands array. It must return one of the values defined in enum CP_STAT.
-----
return codes:
0	success
-1	invalid command
-2	the preprocessing handler returned NULL
-3	the automatic option handling failed (see CP_handle_options)
-4	the main command handler returned CP_ERROR
*/
{
	CP_CURRENT_COMMAND cc;
	_conststring_ option;
	int j;

	cc=CP_getcommand(line,known_commands,verbose);
	if(cc.cmd_index==-1) return(0);		/* empty command */
	if(cc.cmd_index==-2) return(-1);	/* invalid command */

	/* call the preprocessing handler if it exists */
	if(cc.cmd->preproc) {
		option=cc.cmd->preproc(&cc,cc.opt_start);
		if(option==NULL) {
			if(verbose) fprintf(verbose,"CParse error: Preprocessing handler for command '%s' failed.\n",cc.cmd->name);
			return(-2);
		}
		cc.opt_start=cc.position=option;
	}

	/* do the automatic option processing */
	if(CP_handle_options(&cc,verbose)!=0) return(-3);

	/* call the main handler if it exists */
	if(cc.cmd->handler) {
		j=cc.cmd->handler(cc.cmd_index);
		if(j==CP_ERROR) {
			if(verbose) fprintf(verbose,"CParse error: Main handler for command '%s' failed.\n",cc.cmd->name);
			return(-4);
		}
	}

	return(0);
}

