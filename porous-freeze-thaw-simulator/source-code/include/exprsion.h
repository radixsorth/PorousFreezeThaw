/*
MATHEMATICAL EXPRESSION EVALUATOR, VERSION 3.1
COMMON C++ COMPILER EXTENSION
MODIFIED TO WORK WITH strings.h FOR gcc COMPILER; COMPILES WITH g++
(C) 2002-2009 Digithell International
---------------------------------------------------------
INCLUDES HEADER:
strings.h

INCLUDES CLASS:
EXPRESSION

DEBUGGING AND DEVELOPMENT NOTICE:
---------------------------------
FUNCTION AND VARIBLE DEFINITIONS CAN BE FOUND IN SEPARATE
FILES. FUNCTIONS DESCRIPTION IS PROVIDED IN BOTH THIS HEADER FILE
AND THE RESPECTIVE SEPARATE FILES. REFERENCES TO THE RESPECTIVE FILES
ARE LOCATED NEXT TO THE FUNCTION PROTOTYPES.

---------------------------------------------------------
USAGE:
------
  THIS SYSTEM ALLOWS YOU TO EVALUATE MATHEMATICAL EXPRESSIONS AT RUN-TIME.
IT USES VARIABLES, CONSTANTS AND OPERATORS, THAT CAN BE ALL DEFINED,
UNDEFINED OR REDEFINED BY USER, USING THE APPROPRIATE MEMBER FUNCTION.
ITS LEXICAL ANALYSIS IS CASE SENSITIVE.
  TO USE THE EXPRESSION EVALUATOR, CREATE AN INSTANCE OF THE class EXPRESSION,
FOR EXAMPLE "EXPRESSION eval(6);" THE ARGUMENT PASSED TO THE CONSTRUCTOR IS
THE AMOUNT OF EXTRA SPACE TO ALLOCATE FOR ADDITIONAL USER-DEFINED IDENTIFIERS.
THE DEFAULT VALUE FOR IT IS 0. AFTER DECLARATION, THE INSTANCE CAN HANDLE THE
STANDARD OPERATIONS AND CONSTANTS, THAT ARE LISTED BELOW (ALL ARE KEYWORDS):

OPERATIONS:
+ ,- ,* ,/ ,^(power),abs x, int x, floor x, ceil x, round x, sin x,cos x,tan x,
asin x,acos x,atan x,sinh x,cosh x,tanh x,asinh x,acosh x,atanh x,log x,ln x,sqrt x,
x root y,exp x (e^x),pow10 x (10^x), x C y (combination), x P y (permutation),
x! (factorial), toDeg x (converts to degrees),toRad x (converts to radians), rand x

CONSTANTS:
pi=M_PI, e=M_E (CONSTANTS DEFINED IN math.h)

HANDLING TOKENS (SETUP MODE):
-----------------------------
  THE MEMBER FUNCTIONS Set AND Remove ARE USED TO HANDLE BOTH EXISTING
AND NEW USER-DEFINED IDENTIFIERS. THERE ARE TWO MODES FOR DOING SO:
IN SETUP MODE, WHICH IS ENTERED USING THE Enter_Setup MEMBER FUNCTION,
YOU CAN HANDLE ALL IDENTIFIERS INCLUDING TOKENS MARKED AS KEYWORDS.
MOREOVER, ANY NEW ADDED TOKEN WILL BE TREATED AS A KEYWORD. (THUS VARIABLES
BECOME CONSTANTS). AFTER YOU FINISH ALL ACTIONS ON KEYWORDS, YOU SHOULD
LEAVE SETUP USING THE Exit_Setup MEMBER FUNCTION. THEN, NORMAL IDENTIFIERS
WILL BE ACCESSIBLE AGAIN FOR REMOVING AND REDEFINING. AFTER A CALL TO
THE CONSTRUCTOR, THE EVALUATOR IS IN NORMAL MODE.

OPERATOR HANDLERS AND ERRORS:
-----------------------------
  YOU CAN HAVE ONE UNARY AND ONE BINARY OPERATOR WITH THE SAME NAME DEFINED.
(FOR EXAMPLE UNARY '-' AND BINARY '-'). NOTE THAT YOU CANNOT HAVE ONE
PREFIX AND ONE POSTFIX UNARY OPERATOR WITH THE SAME NAME.

  THE USER DEFINED OPERATOR HANDLERS MUST STORE THE ERROR CODE OF THEIR
RESULT TO THE LOCATION POINTED TO BY THEIR LAST PARAMETER. THEY DON'T HAVE
TO STORE THE NO_ERROR VALUE, SINCE IT IS THE DEFAULT. THEY MUST NOT STORE
E_SYNTAX OR E_STACK, SINCE THEY ARE ONLY USED BY THE EVALUATOR ITSELF. A REAL
MATH ERROR SHOULD NOT OCCUR BECAUSE THE matherr FUNCTION IS NOT HOOKED AND NO
SIGNALS ARE HANDLED. THE HANDLERS MUST RECOGNIZE BAD ARGUMENTS BEFORE THEY
PERFORM THE ACTUAL CALCULATION.

  HOWEVER, OVERFLOW ERRORS ARE DIFFICULT TO RECOGNIZE AND THUS THE CALCULATION
MUST BE PERFORMED FIRST AND THEN WE CAN STORE E_OVERFLOW DEPENDING ON THE
STATE OF errno (COMMON ERROR VARIABLE DEFINED IN SEVERAL HEADER FILES).
SOME FUNCTIONS IN THE MATH LIBRARY SET errno=ERANGE IF THEIR RESULT IS
OUT OF RANGE.
  IT IS UP TO THE PROGRAMMER TO HOOK matherr FUNCTION TO AVOID DEFAULT
ERROR MESSAGES FROM BEING DISPLAYED AND TO HANDLE SIGFPE TO AVOID PROGRAM
TERMINATION WHEN FLOATING POINT EXCEPTION OCCURS.

  IF ANY (HANDLED) ERROR OCCURS DURING THE CALCULATION, THE RESULT IS 0. THE
ERROR CODE OF THE LAST CALCULATION IS STORED IN EXPRESSION::Error AND THE
LOCATION IN THE SOURCE STRING AFTER THE CHARACTER WHERE IT OCCURED IN
EXPRESSION::Location. E_SYNTAX ERROR AND APPROPRIATE LOACTION ARE STORED
ALSO AFTER Parse FUNCTION REACHES AN UNKNOWN OR NOT ALLOWED SYMBOL.

FORMATTING THE EXPRESSION:
--------------------------
  TO EVALUATE AN EXPRESSION 4*(5+2), USE THE SYNTAX: eval("4*(5+2)"), WHERE
eval IS THE INSTANCE OF class EXPRESSION. YOU CAN FIND MORE INFORMATION
ABOUT EVALUATION FEATURES AND PRE-PARSING AT Parse, Eval AND operator()
MEMBER FUNCTION DEFINITIONS.
  YOU CAN HAVE MORE INSTANCES OF class EXPRESSION DECLARED, EACH WITH
DIFFERENT SETS OF IDENTIFIERS DEFINED.
  THE TWO ALPHANUMERIC ELEMENTS IN THE EXPRESSION MUST BE DIVIDED BY ONE OR
MORE SPACES. PARENTHESES AND ONE-SPECIAL-CHARACTER-IDENTIFIERS ALSO DIVIDE
LEXICAL ELEMENTS. (FOR EXAMPLE +,-,*,/,^ ETC.) IDENTIFIER IS EITHER A SPECIAL
CHARACTER EXCEPT PARENTHESES, DECIMAL POINT AND SPACE OR AN ALPHANUMERIC
CHARACTER STRING BEGINNING WITH A LETTER. UNDERSCORE IS ALSO TREATED AS A
LETTER. ALL OTHER LEXICAL ELEMENTS ARE NUMBERS IN FLOATING POINT FORMAT.
INVALID LETTERS IN NUMBERS ARE IGNORED.

DEALING WITH MULTIPLE INSTANCES OF class EXPRESSION:
----------------------------------------------------
  EACH INSTANCE CARRIES ITS OWN MEMORY STRUCTURES SO THAT SIMULTANEOUS USE
OF DIFFERENT INSTANCES OF class EXPRESSION IN DIFFERENT THREADS IS SAFE.
LIKEWISE, USING A NESTED EVALUATOR WITHIN THE OPERATOR HANDLER IS POSSIBLE.
THE ONLY EXCEPTION IS THE rand UNARY OPERATOR, WHICH MAKES USE OF A STATIC
POINTER TO IDENTIFY THE ASSOCIATED EVALUATOR CLASS INSTANCE. HOWEVER, USING
RANDOM NUMBER GENERATOR FROM THE C LIBRARY IS BY ITSELF NOT THREAD SAFE.

IF YOU KNOW THAT CONCURRENT EVALUATOR EXECUTION IN DIFFERENT THREADS WILL
OCCUR IN YOUR APPLICATION, YOU SHOULD PREVENT THE USERS FROM USING THE
rand OPERATOR IN THEIR FORMULAS BY EXPLICITLY UNREGISTERING IT USING
THE Remove() MEMBER FUNCTION.
*/
#if !defined(__EXPRSION)
#define __EXPRSION

//-----------------------INCLUDE FILES----------------------

#include "strings.h"

//-------------------------DEFINITIONS-----------------------
#define VSTACK_SIZE 30		// value stack size
#define OPSTACK_SIZE 60		// operator stack size
//---------------GLOBAL VARIABLES AND CLASSES---------------

enum EVAL_ERROR
{
 NO_ERROR,		// successful evaluation
 E_DOMAIN,		// argument not in domain of operator or function
 E_DIVZERO,		// division by zero
 E_OVERFLOW,		// result is >MAXDOUBLE
 E_UNDERFLOW,		// reslut is <MINDOUBLE
 E_TLOSS,		// total loss of significant digits
 // constants used by the expression evaluator itself:
 E_SYNTAX,		// invalid identifier/syntax error
 E_STACK,		// operator or value stack overflow
 E_PRELOAD		// Eval called when PreLoad buffer is empty
			// or Parse() resp. operator() called and the
			// expression was too long (preload table full)
};

class EXPRESSION
{

 // this is needed for some compilers in order to allow nested classes
 // to access the 'outer' class private members
 struct IDENTIFIER;
 struct OP_STACK;
 struct PRE_LOAD;

 friend struct EXPRESSION::IDENTIFIER;
 friend struct EXPRESSION::OP_STACK;
 friend struct EXPRESSION::PRE_LOAD;

 // data:
 enum TYPE_ID { VAR,UNARY,POSTFIX,BINARY,L_PAR,R_PAR,NUMBER };
 unsigned char Set_Up;		// nonzero if the evaluator is in setup mode

 struct IDENTIFIER
 {
  char name[33];
  TYPE_ID type : 7;	// one of { VAR,UNARY,BINARY}
  unsigned char keyword : 1;	// is set when the identifier is a keyword
				// (this means it can be modified in
				// setup mode only)
  char precedence;
  union
  {
   double value;
   double (* unary_op)(double,EVAL_ERROR *);
   double (* binary_op)(double,double,EVAL_ERROR *);
  };
 } * identT;	// pointer to table of identifiers

 int allocated;		// maximum number of identifiers
 int identifiers;	// current number of registered identifiers

 // evaluation stacks:
 double VStack[VSTACK_SIZE];	// numeric value stack
 struct OP_STACK {
  TYPE_ID type;
  char precedence;
  union
  {
   double (* unary_op)(double,EVAL_ERROR *);
   double (* binary_op)(double,double,EVAL_ERROR *);
  };
 } OpStack[OPSTACK_SIZE];	// command stack

 // pre-analysed elements table:
 struct PRE_LOAD {
  TYPE_ID type;
  int loc;				// location in the source string
  union					// in case error position is needed
  {
   IDENTIFIER *ident;			// address of the IDENTIFIER structure
   double value;			// or value of a constant number
  };				        // (references over address allow user
 } * PreLoad;			// to redefine variables and operators
					// with no need to re-analyze
					// the expression)

 unsigned Vptr,Optr;			// indexes of the stack elements
 unsigned Psize,Pelements;		// size of the PreLoad table and
					// current number of elements saved

 union {				// pseudorandom number generator seed
 	long long PRNG_status_long;	// used in the rand function
	unsigned short PRNG_status[3];
 };

 // member functions:
 int Is_Identifier(_conststring_);
 int Defined(_conststring_,int =0);
 IDENTIFIER * Var(_conststring_);
 IDENTIFIER * Unary(_conststring_);
 IDENTIFIER * Binary(_conststring_);

 int Is_Digit(char);
 int Is_Alpha(char);
 int Is_Space(char);
 int Is_Special(char);
 void Eval_Stack(char);

 public:
 EVAL_ERROR Error;	// contains the error code of the last evaluation
 int Location;		// if lasterror!=NO_ERROR, it contains the position
			// in the source string where the error ocurred

 // constructor:
 EXPRESSION(int =0,int =80); // 1st parameter is the maximum number of user
			// added identifiers (addition to the default
			// identifier table size), 2nd parameter determines
			// the size of the element preload table
 // destructor:
 ~EXPRESSION();

 double Random(unsigned long long =0);// an interface to the SVID random number
 					// generator. The seed is used only if nonzero,
					// otherwise the generator is not reseeded. Each
					// instance of EXPRESSION has its own PRNG status storage.
					// Returns values between 0 and 1. Used by the rand
					// function. The user should initialize the PRNG
					// by calling Random(seed) before use, otherwise
					// the PRNG behavior is undefined (may or may not
					// vary across program invocations).

 double operator()(_conststring_);	// evaluates given expression
 int Parse(_conststring_);		// performs lexical analysis only
 double Eval();				// evaluates already lexiacally
					// analysed expression

 // identifier handling member functions:
 void Enter_Setup();			// enters the setup mode
 void Exit_Setup();			// exits the setup mode
 void Reset();				// removes all non-keyword identifiers

 int Set(_conststring_,double);		// set variable
 int Set(_conststring_,double(*)(double,EVAL_ERROR *),char);		 // set unary operator
 int Set(_conststring_,double(*)(double,double,EVAL_ERROR *),char);	 // set binary operator
 int Remove(_conststring_);		// remove identifier
 int GetIndex(_conststring_);		// gets index of a variable in identT
 void SetValue(int,double);		// sets a new value to a variable with
					// the given index
};

//----------------------------------------------------------
// DEFINITION ==> SEE "EXP_ALL.CPP"
// (THIS SOURCE FILE ALSO CONTAINS A LOT OF INFORMATION ABOUT THE USE OF ALL METHODS.)

#endif
