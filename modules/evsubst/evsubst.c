/**************************************************\
* EVSubst: Environment variable value substitution *
* (C) 2007 Pavel Strachota                         *
* file: evsubst.c                                  *
\**************************************************/

#include "evsubst.h"

#include <stdlib.h>

int ev_subst(_string_ target, _conststring_ source)
/*
copies the string 'source' to 'target', substituting values for environment variables.
The user must ensure there is enough space for the resulting string in 'target'.

ev_subst() follows the ideas of the BASH scripting language:
The variable names can appear in the format $VARIABLE_NAME or ${VARIABLE_NAME}. The latter
format is useful if other alphabetic characters follow directly after the variable (there
is no whitespace or other special character that would delimit the variable name).

The variable name can contain the characters A-Z,a-z,0-9 and the underscore. However, it must
not begin with a number. The variable name must consist of at most 127 characters.

Parts of the string enclosed in single quotes are not subject to variable substitution. Single
quotes are not copied to the output. Trailing quotes can be omitted, even though this is not a
common behavior. (For example, in BASH expressions, you need to keep the quotes matched).
Double quotes are treated as ordinary characters. Single quote can be escaped by \'.
In all other cases, the backslash is treated as an ordinary character.


EXAMPLE: Assume we have an environment variable called '$VAR' with the value 'bat'
Then the string
	${VAR}man says "' $VAR'=$VAR"
is translated by ev_subst() to
	batman says " $VAR=bat"

return codes:
number of substituted variables		success
-1					error: reference to an undefined variable
-2					error: invalid variable name

In case of error, the content of 'target' is undefined.
*/
{
	int numVars=0;
	int quoted=0;
	int brace;
	int l;
	char varName[128];
	_string_ vNptr;
	_string_ varValue;

	while(*source) {
		if(*source!='$' || quoted) {
			switch(*source) {
				case '\'':	quoted=1-quoted; break;
				case '\\':	if(*(source+1) == '\'') source++;
				default:	*(target++)=*source;
			}
			source++;
		} else {		/* (possible) beginning of the variable */
			vNptr=varName;
			brace=0;
			source++;
			if(*source=='{') { brace=1; source++; }
			if(*source>='0' && *source<='9') return(-2);

			/* extract variable name */
			l=0;
			while((*source>='a' && *source<='z') || (*source>='A' && *source<='Z') || (*source>='0' && *source<='9') || *source=='_')
				{ l++; *(vNptr++)=*(source++); }
			*vNptr=0;
			if(brace) {
				if(*source!='}' || l==0) return(-2);
				source++;	/* skip the closing brace */
			}
			if(l==0) {		/* there was no variable */
				*(target++)='$';
				continue; /* this also handles the '$' at the very end of the string */
			} else {		/* substitute variable */
				varValue=getenv(varName);
				if(varValue)
					l=set(target,varValue);
				else return(-1);
				numVars++;
				target+=l;
			}
		}
	}
	*target=0;
	return(numVars);
}
