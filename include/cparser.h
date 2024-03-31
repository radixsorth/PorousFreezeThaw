/***********************************************\
* CParse: The command and option parser		*
* (C) 2005, 2015 Pavel Strachota                *
* file: cparser.h                               *
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
(see cparser.h)

HOW IT WORKS: there are several functions that deal with commands. The most complex
version CP_runcommand() parses the line and calls handlers for each option and
for the command itself. The handlers are defined through the CP_COMMAND and
CP_OPTION structures. Note that this toolbox naturally allows multiple occurence
of the same option on the line. It is left up to you if you want to disable this
feature.
*/

#if !defined __cparser
#define __cparser

#include <stdio.h>
#include "strings.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
specifies whether the option allows or needs a value
*/
typedef enum {
	CP_NONE,
	CP_REQUIRED,
	CP_OPTIONAL
} CP_OPT_VALUE;

/*
return values for the command and option handlers
*/
typedef enum {
	CP_SUCCESS,
	CP_ERROR
} CP_STAT;

struct _CP_CURRENT_COMMAND;	/* forward declaration needed for the next typedef */

typedef _conststring_ (*CP_CMD_Preprocess)(struct _CP_CURRENT_COMMAND *, _conststring_);
typedef CP_STAT (*CP_CMD_Handler)(int);
typedef CP_STAT (*CP_OPT_Handler)(int, int, _conststring_);

/*
You should provide an array of these structures for each defined command.
One element of the array specifies one option that is available for use
with the command that the array is associated to. The array must end with
an element that contains a NULL command name.

NOTE: There may be several commands sharing the same option.
*/
typedef struct {
	_conststring_ name;			/* option keyword */
	CP_OPT_VALUE uses_value;	/* specifies whether this option needs a value */
	CP_OPT_Handler handler;		/* an option handler function */
} CP_OPTION;

/*
An array of CP_COMMAND structures should describe all the commands your program
(or its part) is able to handle. Each command (array element) has an associated
list of available options. The array must end with an element that contains
a NULL command name.
*/
typedef struct {
	_conststring_ name;			/* command keyword */
	CP_OPTION * options;		/* a pointer to an array of valid command options:
					   NOTE: If the array is empty (contains just one
					   "terminating" element), anything written after such
					   command will cause the "invalid command" error.
					   However, if you set options to NULL, the scan
					   for options won't be performed at all.
					   (CP_walk_opt() will always return -4)  */
	CP_CMD_Preprocess preproc;	/* a handler called BEFORE option parsing */
	CP_CMD_Handler handler;		/* a handler called AFTER option parsing */
} CP_COMMAND;

/* This is a structure returned by CP_getcommand to fully describe the command
that was found. It is considered read-only and thus you should not modify any of
its elements.
*/
typedef struct _CP_CURRENT_COMMAND {
	int cmd_index;			/* the subscript in the CP_COMMAND array,
					   for use in user-defined handlers only */
	int val_error;			/* the subscript of the option that caused a
					   'value error'. Check this only if the
					   CP_walk_opt() function returns -2 or -3 */
	CP_COMMAND * cmd;		/* the CP_COMMAND structure of the current command */
	_conststring_ opt_start;	/* pointer to the string where the options begin */
	_conststring_ position;		/* pointer to the string where the next option begins */
} CP_CURRENT_COMMAND;

CP_CURRENT_COMMAND CP_getcommand(_conststring_ line, CP_COMMAND * known_commands, FILE * verbose);
/*
scans the given line of text for the known commands. Returns a structure with
command information if a command is found. Returns a special object which has
cmd_index==-1 if the line contains no commands (empty line or remark). Returns
an object which has cmd_index==-2 on error (command does not exist). Does NOT
call any command handling functions. If the stream 'verbose' is not NULL,
CP_getcommand() will output error messages to this stream.
*/

int CP_walk_opt(CP_CURRENT_COMMAND * cc, _string_ opt_value, FILE * verbose);
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

void CP_restart_walk(CP_CURRENT_COMMAND * cc);
/*
rewinds the option pointer of the current command so that the next call to
CP_walk_opt returns the first command option.
*/

int CP_handle_options(CP_CURRENT_COMMAND * cc, FILE * verbose);
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

int CP_runcommand(_conststring_ line, CP_COMMAND * known_commands, FILE * verbose);
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

#ifdef __cplusplus
}
#endif

#endif		/* __cparser */
