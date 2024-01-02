/*
STANDARD STRING OPERATION/CONTROL MODULE:
COMMON C++ COMPILER EXTENSION
MODIFIED TO WORK WITH THE gcc C Language COMPILER
(CAN BE USED WITH C++ AS WELL - FUNCTIONS ARE extern "C")
(C) 1998-2009 Digithell International.
---------------------------------------------------------
INCLUDES HEADERS:
none

INCLUDES FUNCTIONS:
set	        scompare	match ( macro )
len		instr		s_swap
left		val		float_val
right		hexval		float_tostring
add		tostring	sort
mid		makestring	sr_filter
pw10

DEBUGGING AND DEVELOPMENT NOTICE:
---------------------------------
FUNCTION AND VARIBLE DEFINITIONS CAN BE FOUND IN SEPARATE
FILES. FUNCTIONS DESCRIPTION IS PROVIDED IN BOTH THIS HEADER FILE
AND THE RESPECTIVE SEPARATE FILES. REFERENCES TO THE RESPECTIVE FILES
ARE LOCATED NEXT TO THE FUNCTION PROTOTYPES.

THE extern "C" SPECIFICATION DOES NOT NEED TO BE REPEATED IN EACH SEPARATE
.c FILE, BECAUSE THIS HEADER IS ALWAYS INCLUDED. THEREFORE THE COMPILATION
WITH A C++ COMPILER WILL PRODUCE NOT-MANGLED NAMES LIKE THE C COMPILER.
*/
#if !defined(__STRINGS)
#define __STRINGS

/* -----------------------INCLUDE FILES----------------------- */

/* none */

/* -------------------------DEFINITIONS----------------------- */

#define AND &&
#define OR ||
#define NOT !
#define then
#define SENSITIVE 1

typedef char * * stringarray;	/* pointer to an array of pointers to char */
typedef char * _string_;
typedef const char * _conststring_;

/* ---------------------MACRO DEFINITIONS-------------------- */

#define stg(q) ((__stg_buf[0]=(q),__stg_buf[1]=0,__stg_buf))		/* converts character to string */

/*
Note: The stg macro utilizes a static storage space to put the resulting string to.
Its usage is therefore not thread safe! In order to circumvent this diffuculty in
multithreaded applications, use the _stg macro instead. The _stg macro accepts a second
argument of type _string_ identifying the memory location to assemble the string at.
*/
#define _stg(q,buffer) (((buffer)[0]=(q),(buffer)[1]=0,(buffer)))	/* converts character to string */

#define match(str1,str2) (NOT scompare((str1),(str2),SENSITIVE))
/* match RETURNS 0 IF THE COMPARED STRINGS DIFFER OR -1 IF THEY ARE THE SAME */

/*
The 'pow10(x)' function returning 10^x is only an extension to C. runtime library.
Sometimes it has the same purpose as the 'pow(x,y)' function, which returns x^y.
If you want to be sure of what you are really using, use this macro instead.
*/
#define pw10(x) (pow(10,x))
/* ---------------GLOBAL VARIABLES AND CLASSES--------------- */
struct _TOSTRINGBUF
{
char buf[40];
};

/* DEFINITION ==> SEE "STR_VDEF.C" */
extern struct _TOSTRINGBUF tsb;
extern unsigned _maxstrlen;	/* default maximum allowed string length */
extern char __stg_buf[];	/* auxiliary buffer for the stg macro */
extern char _NOSTRING[];	/* the null string is one byte long, so we */
				/* can reduce the program size by referencing */
				/* the same null string at all places */

/*---------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* DEFINITION ==> SEE "STR_LSAM.C" */
unsigned set(_string_ target,_conststring_ source);
/* COPIES STRING source TO PREVIOULSY ALLOCATED ARRAY target */
/* RETURNS LENGTH OF source (IN BYTES). */
/* WARNING: THERE MUST BE ENOUGH SPACE ALLOCATED FOR target! */
/* set COPIES A MAXIMUM OF _maxstrlen BYTES OF source TO target! */

unsigned len(_conststring_ string);
/* RETURNS LENGTH OF string IN BYTES */
/*
AUTOMATICALLY TRUNCATES STRINGS TO _maxstrlen CHARACTERS
EVEN THOUGH THE ARGUMENT IS A POITRER TO CONST !!!
THIS IS JUST A PROTECTION AGAINST UNTERMINATED STRINGS:
IF ALL STRINGS ARE SHORT ENOUGH, NOTHING INTERESTING HAPPENS.
*/


void add(_string_ string1,_conststring_ string2);
/* ADDS string2 TO string1 */
/* DOESN'T ADD ANYTHING IF THE LENGTH AFTER THE OPERATION WOULD EXCEED */
/* THE RANGE GIVEN BY _maxstrlen */

void makestring(_string_ destination,char character,unsigned n);
/* CREATES THE string OF n characters */

/* DEFINITION ==> SEE "STR_LRM.C" */
void left(_string_ target,_conststring_ source,unsigned number);
/* COPIES FIRST number CHARACTERS OF source INTO target */

void right(_string_ target,_conststring_ source,unsigned number);
/* COPIES LAST number CHARACTERS OF source INTO target */

void mid(_string_ target,_conststring_ source,unsigned start,unsigned end);
/* COPIES start-TH TO end-TH CHARACTER OF source INTO target */
/* USE start == 1 TO ACCESS THE FIRST CHARACTER IN A STRING */

/* DEFINITION ==> SEE "STR_SWP.C" */
int s_swap(_string_ string1,_string_ string2);
/* SWAPS TWO STRINGS UP TO _maxstrlen CHARACTERS DIRECTLY IN MEMORY */
/* IF ONE OF THE STRINGS' LENGTHS EXCEEDS _maxstrlen, ONLY THE FIRST */
/* _maxstrlen CHARACTERS ARE SWAPPED. */
/* RETURNS 0 ON SUCCESS AND -1 ON MEMORY ALLOCATION ERROR */
/* WARNING: THERE MUST BE ENOUGH ALLOCATED SPACE IN ALL TWO STRINGS, */
/* OR THE SHORTER ONE WILL BE REPLACED INCORRECTLY (CAUSING MEMORY PROBLEMS) */

/* DEFINITION ==> SEE "STR_CMP.C" */
int scompare(_conststring_ string1,_conststring_ string2,char mode);
/* COMPARES string1 AND string2. */
/* RETURNS POSITIVE NUMBER IF string1 > string2 ( ALPHABETICALLY SORTED ) */
/* OR NEGATIVE IF string1 < string2 OR 0 IF THEY ARE THE SAME */
/* scompare DOESN'T CARE OF _maxstrlen TO ACHIEVE HIGHER SPEED !!! */
/* mode DETERMINES WHETHER THE COMPARISON IS CASE SENSITIVE (SENSITIVE) OR NOT */
/* (NOT SENSITIVE) , ! SENSITIVE RESPECTIVELY. */

/* DEFINITION ==> SEE "STR_INS.C" */
unsigned instr(_conststring_ source,_conststring_ pattern,unsigned position,char mode);
/* SEARCHS pattern STRING IN source STRING FROM position (MIN 1) */
/* RETURNS POSITION OF THE BEGINNING OF THE pattern STRING */
/* IF source DOES NOT CONTAIN pattern, RETURNS 0 */
/* IF pattern IS NULL, RETURNS position */
/* IF source IS NULL, RETURNS 0 */
/* mode DETERMINES WHETHER THE SEARCH IS CASE SENSITIVE (SENSITIVE) OR NOT */
/* (NOT SENSITIVE) , ! SENSITIVE RESPECTIVELY. */

/* DEFINITION ==> SEE "STR_VAL.C" */
long val(_conststring_ string);
/* CONVERTS string TO A NUMERIC VALUE (int or unsigned TYPE - */
/* - long TYPE CAN SUPPLY BOTH) */
/* STRING CAN CONTAIN NEGATION SIGN (-) AT THE FIRST PLACE AND */
/* DECIMAL DIGITS (0-9). OTHERWISE, THE FUNCTION SKIPS ALL OTHER CHARACTERS */
/* AND USES VALID DIGITS ONLY. IF NO VALID DIGITS ARE FOUND, 0 IS RETURNED */

/* DEFINITION ==> SEE "STR_HVAL.C" */
unsigned long hexval(_conststring_ string);
/* CONVERTS string TO A NUMERIC VALUE (unsigned (!) long TYPE) */
/* THE STRING IS ASSUMED TO BE IN A HEXADECIMAL FORMAT, i.e. IT MAY */
/* CONTAIN HEX DIGITS (0-9,A-F) BOTH UPPERCASE AND LOWERCASE. */
/* OTHERWISE, THE FUNCTION SKIPS ALL OTHER CHARACTERS AND USES */
/* VALID DIGITS ONLY. IF NO VALID DIGITS ARE FOUND, 0 IS RETURNED */

/* HINT: THE CHARACTER IGNORATION FEATURE LETS YOU PASS STRINGS LIKE */
/* "0xFFFF" AND "034h" TO hexval. */

/* DEFINITION ==> SEE "STR_FVAL.C" */
double float_val(_conststring_ string);
/* CONVERTS string TO A NUMERIC VALUE (double TYPE) */
/* STRING CAN CONTAIN NEGATION SIGN (-) AT THE FIRST PLACE, */
/* DECIMAL DIGITS (0-9),ONE DECIMAL POINT (.), */
/* OR ONE EXPONENT EXPRESSION (E+-##). */
/* (.) MUST BE BEFORE (E+-##).OTHERWISE, ALL OTHER CHARACTERS */
/* ARE SKIPPED (!). IF NO DIGITS ARE FOUND, 0 IS RETURNED. */

/* DEFINITION ==> SEE "STR_TST.C" */
_conststring_ tostring(long value);
/* CONVERTS THE long TYPE VALUE TO THE STRING AND RETURNS POINTER */
/* TO THIS STRING. THE STRING IS STATIC AND IS OVERWRITTEN UPON EACH */
/* CALL TO tostring OR float_tostring. */

/* DEFINITION ==> SEE "STR_FTST.C" */
_conststring_ float_tostring(double value,int ndigits,int expfrom);
/* CONVERTS THE double TYPE VALUE TO THE STRING AND RETURNS POINTER */
/* TO THIS STRING. THE STRING IS STATIC AND IS OVERWRITTEN UPON EACH */
/* CALL TO tostring OR float_tostring. ndigits DETERMINES THE MAXIMUM NUMBER */
/* OF DIGITS TO BE OUTPUT AND IS IN THE RANGE 1-17. NUMBERS >=10^ndigits OR */
/* <=10^-expfrom ARE OUTPUT IN THE SCIENTIFIC NOTATION. */

/* DEFINITION ==> SEE "STR_SORT.C" */
int sort(stringarray strings,int startstring,int endstring);
/* PERFORMS SIPMPLE ASCII ALPHABETICAL BUBBLESORT IN AN ARRAY OF STRINGS, */
/* SORTING THE SMALLEST ASCII CODE TO THE BEGINNING. */
/* SORTING STARTS ON STRING startstring AND ENDS ON endstring */
/* RETURNS 0 ON SUCCESS AND -1 ON ERROR */
/* TO SORT LARGE AMOUNTS OF DATA, USE THE QuickSort FUNCTION (qsort.h) */

/* DEFINITION ==> SEE "STR_SRF.C" */
int sr_filter(stringarray strings,_conststring_ pattern,int startstring,int endstring);
/* SEARCHS FOR A PATTERN IN THE ARRAY OF STRINGS AND LOCATES THE STRINGS, */
/* THAT INCLUDE THE PATTERN, TO THE BEGINNING OF THE ARRAY */
/* RETURNS THE NUMBER OF PATTERNS FOUND */
/* RETURNS -1 ON ERROR */

#ifdef __cplusplus
}
#endif

#endif		/* __STRINGS */
