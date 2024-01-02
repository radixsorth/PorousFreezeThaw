/*
EXP_ALL.CPP (EXPRSION.H)
class EXPRESSION: DEFINITION MODULE OF EVERYTHING
COMMON C++ COMPILER EXTENSION
MODIFIED TO WORK WITH strings.h FOR gcc COMPILER; COMPILES WITH g++
(C) 2002-2007 Digithell, Inc.
*/

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "exprsion.h"

static EXPRESSION * this_e;	// USED IN THE OPERATOR HANDLERS TO ACCESS THE CALLING INSTANCE
				// OF THE EVALUATOR. USER-DEFINED OPERATORS CAN NOT TAKE ADVANTAGE
				// OF THIS FEATURE.

// DEFAULT OPERATOR HANDLER FUNCTIONS:
double __Plus__(double x,double y,EVAL_ERROR *)
{
 return(x+y);
}

double __UPlus__(double x,EVAL_ERROR *)
{
 return(x);
}

double __Minus__(double x,double y,EVAL_ERROR *)
{
 return(x-y);
}

double __UMinus__(double x,EVAL_ERROR *)
{
 return(-x);
}

double __Mul__(double x,double y,EVAL_ERROR *)
{
 return(x*y);
}

double __Div__(double x,double y,EVAL_ERROR *e)
{
 if(y==0) { *e=E_DIVZERO; return(0); }
 return(x/y);
}

double __Power__(double x,double y,EVAL_ERROR *e)
{
 double r,n;
 if(x==0 AND y<=0) { *e=E_DOMAIN; return(0); }
 errno=0;
 if(x<0) {
  if(y!=floor(y))	 // y must be either integer
   if(fmod(1/y-1,2)!=0) { *e=E_DOMAIN; return(0); }	// or 1/(2n+1), where n is integer!=0
   else { r=-pow(-x,y);	goto finish; }	// pow can not compute the last case by itself
 } r=pow(x,y);
 finish:
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); }
 return(r);
}

double __rand__(double x, EVAL_ERROR *e)
{
	return(this_e->Random((unsigned long long)x));
}

double __root__(double x,double y,EVAL_ERROR *e)
{
 if(x==0) { *e=E_DOMAIN; return(0); }
 return(__Power__(y,1/x,e));
}

double __sqrt__(double x,EVAL_ERROR *e)
{
 if(x<0) { *e=E_DOMAIN; return(0); }
 return(sqrt(x));
}

double __exp__(double x,EVAL_ERROR *e)
{
 errno=0;
 double r=exp(x);
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); }
 return(r);
}

double __ln__(double x,EVAL_ERROR *e)
{
 if(x<=0) { *e=E_DOMAIN; return(0); }
 return(log(x));
}

double __pow10__(double x,EVAL_ERROR *e)
{
 if(x>308) { *e=E_OVERFLOW; return(0); }
 return(pow(10,x));
}

double __log__(double x,EVAL_ERROR *e)
{
 if(x<=0) { *e=E_DOMAIN; return(0); }
 return(log10(x));
}

double __xabs__(double x,EVAL_ERROR *)
{
 return(fabs(x));
}

double __xint__(double x,EVAL_ERROR *)
{
 if(x>0) return(floor(x));
 else return(ceil(x));
}

double __ceil__(double x,EVAL_ERROR *)
{
 return(ceil(x));
}

double __floor__(double x,EVAL_ERROR *)
{
 return(floor(x));
}

double __round__(double x,EVAL_ERROR *)
{
 double r=floor(x);
 if(x-r>=0.5) r+=1;
 return(r);
}

double __sin__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>1e12) { *e=E_TLOSS; return(0); }
 return(sin(x));
}

double __cos__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>1e12) { *e=E_TLOSS; return(0); }
 return(cos(x));
}

double __tan__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>1e12) { *e=E_TLOSS; return(0); }
 if(cos(x)==0) { *e=E_DOMAIN; return(0); }
 return(tan(x));
}

double __asin__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>1) { *e=E_DOMAIN; return(0); }
 return(asin(x));
}

double __acos__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>1) { *e=E_DOMAIN; return(0); }
 return(acos(x));
}

double __atan__(double x,EVAL_ERROR *)
{
 return(atan(x));
}

double __sinh__(double x,EVAL_ERROR *e)
{
 errno=0;
 double r=sinh(x);
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); }
 return(r);
}

double __cosh__(double x,EVAL_ERROR *e)
{
 errno=0;
 double r=cosh(x);
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); }
 return(r);
}

double __tanh__(double x,EVAL_ERROR *e)
{
 errno=0;
 double r=tanh(x);
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); }
 return(r);
}

// inverse hyperbolic functions are not a part of math.h
double __asinh__(double x,EVAL_ERROR *e)
{
 errno=0;
 double r=log(x+sqrt(pow(x,2)+1));
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); } // because of pow(x,2)
 return(r);
}

double __acosh__(double x,EVAL_ERROR *e)
{
 if(x<1) { *e=E_DOMAIN; return(0); }
 errno=0;
 double r=log(x+sqrt(pow(x,2)-1));
 if(errno==ERANGE) { *e=E_OVERFLOW; return(0); } // because of pow(x,2)
 return(r);
}

double __atanh__(double x,EVAL_ERROR *e)
{
 if(fabs(x)>=1) { *e=E_DOMAIN; return(0); }
 return(log((1+x)/(1-x))/2);
}

double __Fact__(double x,EVAL_ERROR *e)
{
 if(x<0 OR x!=floor(x)) { *e=E_DOMAIN; return(0); }
 if(x>170) { *e=E_OVERFLOW; return(0); }

 double r=1;
 while(x) r*=x--;
 return(r);
}

double __P__(double x,double y,EVAL_ERROR *e)
{
 if(x<0 OR x!=floor(x) OR y<0 OR y!=floor(y) OR x<y) { *e=E_DOMAIN; return(0); }

 double r=1;
 while(y) { r*=x--; y--; }
 return(r);
}

double __C__(double x,double y,EVAL_ERROR *e)
{
 if(x<0 OR x!=floor(x) OR y<0 OR y!=floor(y) OR x<y) { *e=E_DOMAIN; return(0); }

 double r=1;
 while(y) { r*=x--; r/=y--; }
 return(r);
}

double __toDeg__(double x,EVAL_ERROR *)
{
 return(x/M_PI*180);
}

double __toRad__(double x,EVAL_ERROR *)
{
 return(x/180*M_PI);
}

//----------------------------------------------------------
// PRIVATE MEMBER FUNCTIONS:

int EXPRESSION::Is_Digit(char c)
{
 if((c>='0' AND c<='9') OR c=='.') return(1);
 return(0);
}

int EXPRESSION::Is_Alpha(char c)
{
 c=toupper(c);
 if((c>='A' AND c<='Z') OR c=='_') return(1);
 return(0);
}

int EXPRESSION::Is_Space(char c)
{
 if(c==' ') return(1);
 return(0);
}

int EXPRESSION::Is_Special(char c)
{
 if(Is_Digit(c) OR Is_Alpha(c) OR Is_Space(c) OR c=='(' OR c==')') return(0);
 return(1);
}

int EXPRESSION::Is_Identifier(_conststring_ s)
// checks whether the string is a valid identifier name
// returns 1 on success, otherwise returns 0
{
 int q,l=len(s);
 if(l==1 AND Is_Special(*s)) return(1);		// only one special character

 if(NOT Is_Alpha(*s)) return(0);		// first must be a letter
						// this also handles null string
 for(q=1;q<l;q++)
  if(s++, (NOT Is_Alpha(*s) AND NOT Is_Digit(*s)) ) return(0); // invalid character encountered
 return(1);
}

int EXPRESSION::Defined(_conststring_ s,int fromwhere)
// checks whether the string is a defined identifier (symbol)
// it browses through the table starting at the position fromwhere
// returns its index in the table or
// -1 if not found or -2 if invalid identifier name
{
 if(len(s)>32 OR NOT Is_Identifier(s)) return(-2);

 int q;
 for(q=fromwhere;q<identifiers;q++)
  if(match(identT[q].name,s)) return(q);
 return(-1);
}

EXPRESSION::IDENTIFIER * EXPRESSION::Var(_conststring_ s)
// checks whether the string is a defined variable
// returns its address on success, otherwise returns NULL
{
 int q;
 if((q=Defined(s))<0) return(NULL);
 if(identT[q].type==VAR) return(identT+q);
 return(NULL);
}

EXPRESSION::IDENTIFIER * EXPRESSION::Unary(_conststring_ s)
// checks whether the string is a defined unary operator
// returns its address on success, otherwise returns NULL
{
 int q;
 if((q=Defined(s))<0) return(NULL);
 if(identT[q].type==UNARY) return(identT+q);
 if(identT[q].type!=BINARY) return(NULL);	// not an operator.
 if((q=Defined(s,q+1))<0) return(NULL);		// possibly the second version is unary
 return(identT+q);
}

EXPRESSION::IDENTIFIER * EXPRESSION::Binary(_conststring_ s)
// checks whether the string is a defined binary operator
// returns its address on success, otherwise returns NULL
{
 int q;
 if((q=Defined(s))<0) return(NULL);
 if(identT[q].type==BINARY) return(identT+q);
 if(identT[q].type!=UNARY) return(NULL);	// not an operator.
 if((q=Defined(s,q+1))<0) return(NULL);		// possibly the second version is binary
 return(identT+q);
}

void EXPRESSION::Eval_Stack(char precedence)
// evaluates the stack until it reaches an operator with lower
// precedence than the given precedence or a left parenthesis.
// if precedence==32, it closes (removes) one parenthesis.
// if precedence>32, it evaluates the whole stack (closes all parentheses).
{
 OP_STACK op;
 while(Optr)
 {
  op=OpStack[Optr-1];
  if(op.type!=L_PAR AND op.precedence>precedence) break;
  if(op.type==L_PAR AND precedence<32) break;
  Optr--;
  if(op.type==L_PAR AND precedence==32) break;	// parenthesis popped
  else if(op.type==UNARY)
   VStack[Vptr-1]=op.unary_op(VStack[Vptr-1],&Error);
  else if(op.type==BINARY) {
   Vptr--;
   VStack[Vptr-1]=op.binary_op(VStack[Vptr-1],VStack[Vptr],&Error);
  }
  if(Error) return;
 }
}

//----------------------------------------------------------
// PUBLIC MEMBER FUNCTIONS:

EXPRESSION::EXPRESSION(int ident_no,int preload_size)
// ident_no REPRESENTS THE AMOUNT OF EXTRA SPACE TO ALLOCATE FOR THE
// IDENTIFIER TABLE. THE EXTRA SPACE THEN CAN BE USED TO ADD USER-DEFINED
// IDENTIFIERS.
// THE PRECEDENCE OF THE DEFAULT OPERATORS IS ALWAYS AN EVEN NUMBER AND
// IS IN THE RANGE 10-22.
: identifiers(0),Pelements(0)
{
 int q;

 try {
 	PreLoad=new PRE_LOAD[preload_size];
	Psize=preload_size;
 } catch(...) {
	Psize=0;
 }

 try {
 	identT=new IDENTIFIER[ident_no+38];

	allocated=ident_no+38;
	for(q=0;q<allocated;q++)
	set(identT[q].name,_NOSTRING);
	// -------------
	Enter_Setup();
	Set("pi",M_PI);
	Set("e",M_E);

	Set("-",__Minus__,22);
	Set("+",__Plus__,22);
	Set("*",__Mul__,20);
	Set("/",__Div__,20);

	Set("C",__C__,18);
	Set("P",__P__,18);

	Set("-",__UMinus__,16);
	Set("+",__UPlus__,16);
	Set("int",__xint__,16);
	Set("floor",__floor__,16);
	Set("ceil",__ceil__,16);
	Set("round",__round__,16);
	Set("abs",__xabs__,16);
	Set("sin",__sin__,16);
	Set("cos",__cos__,16);
	Set("tan",__tan__,16);
	Set("asin",__asin__,16);
	Set("acos",__acos__,16);
	Set("atan",__atan__,16);
	Set("sinh",__sinh__,16);
	Set("cosh",__cosh__,16);
	Set("tanh",__tanh__,16);
	Set("asinh",__asinh__,16);
	Set("acosh",__acosh__,16);
	Set("atanh",__atanh__,16);
	Set("log",__log__,16);
	Set("ln",__ln__,16);
	Set("sqrt",__sqrt__,16);
	Set("exp",__exp__,16);
	Set("pow10",__pow10__,16);
	Set("rand",__rand__,16);

	Set("^",__Power__,14);
	Set("root",__root__,14);

	Set("!",__Fact__,-12);

	Set("toDeg",__toDeg__,10);
	Set("toRad",__toRad__,10);
	Exit_Setup();
 } catch(...) {
	allocated=0;
 }
}

EXPRESSION::~EXPRESSION()
{
 if(allocated>0) delete [] identT;
 if(Psize>0) delete [] PreLoad;
}


double EXPRESSION::Random(unsigned long long seed)
// RETURNS A PSEUDORANDOM NUMBER IN THE RANGE 0-1. THIS IS THE
// INTERFACE TO THE SVID RANDOM NUMBER GENERATOR. THE STATUS OF THE
// PRNG IS RE-SEEDED ONLY IF seed IS NONZERO. IN FACT, ONLY THE LOWER
// 48 BITS ARE USED FOR SEED. EACH INSTANCE OF class EXPRESSION HOLDS
// ITS OWN PRNG STATUS. SEE THE CLASS DEFINITION FOR MORE DETAILS.
{
	if(seed) PRNG_status_long=seed;
	return(erand48(PRNG_status));
}

void EXPRESSION::Enter_Setup()
// ENTERS SETUP MODE. IN THIS MODE, YOU CAN MODIFY AND REMOVE TOKENS
// MARKED AS KEYWORDS. NEW ADDED IDENTIFIERS ARE ALSO TREATED AS KEYWORDS.
{
 Set_Up=1;
};

void EXPRESSION::Exit_Setup()
// EXITS SETUP MODE. YOU WILL NO LONGER BE ABLE TO MODIFY OR REMOVE TOKENS
// MARKED AS KEYWORDS
{
 Set_Up=0;
};

void EXPRESSION::Reset()
// REMOVES ALL NON-KEYWORD (NON-CONSTANT) VARIABLES AND OPERATORS
// (DOES NOT HAVE TO CARE OF TWO OPERATOR VERSIONS AS THEY ARE BOTH KEYWORDS
// OR BOTH NON-KEYWORDS, WHICH IS ENSURED BY THE SET METHOD)
{
 Pelements=0;
 int i=0;

 while(i<identifiers) {
  if(NOT identT[i].keyword) {
   identifiers--;
   identT[i]=identT[identifiers];
  } else i++;
 }
}

int EXPRESSION::Set(_conststring_ identifier,double value)
// ADDS OR REDEFINES A VARIABLE
// RETURNS 0 ON SUCCESS
// -1 IF INVALID IDENTIFIER
// -2 IF TABLE FULL
// -3 IF IDENTIFIER ALREADY EXISTS AND IS NOT OF TYPE 'VAR'
// -4 IF IDENTIFIER ALREADY EXISTS AND IS A KEYWORD (IF NOT IN SETUP MODE)
{
 IDENTIFIER * i;

 if(NOT Is_Identifier(identifier)) return(-1);

 if(Defined(identifier)>=0)
  if(NOT (i=Var(identifier))) return(-3);
  else if(i->keyword AND NOT Set_Up) return(-4);
  else {
   i->value=value;			// redefine variable
   return(0);
  }

 if(identifiers==allocated) return(-2);
 set(identT[identifiers].name,identifier);	// add a variable
 identT[identifiers].type=VAR;
 identT[identifiers].keyword=Set_Up;
 identT[identifiers].value=value;
 identifiers++;
 Pelements=0;
 return(0);
}

int EXPRESSION::Set(_conststring_ identifier,double (* un_op)(double,EVAL_ERROR *),char precedence)
// ADDS OR REDEFINES A UNARY OPERATOR WITH A GIVEN EVALUATION PRECEDENCE.
// POSITIVE PRECEDENCE (OR 0) DEFINES A PREFIX OPERATOR, NEGATIVE PRECEDENCE
// MEANS A POSTFIX OPERATOR.
// PRECEDENCE MUST BE A NUMBER IN THE RANGE 0-31, WHERE HIGHER VALUES MEAN
// LOWER PRECEDENCE. PREFIX UNARY OPERATOR IS ALWAYS PUSHED ONTO THE OPERATOR
// STACK WITHOUT ANY EVALUATION AND ITS PRECEDENCE IS MEANINGFUL ONLY WHEN
// ANOTHER BINARY OR POSTFIX UNARY OPERATOR IS TO BE ADDED. POSTFIX UNARY
// OPERATOR IS, ON THE CONTRARY, ALWAYS EVALUATED IMMEDIATELY.
// RETURNS 0 ON SUCCESS
// -1 IF INVALID IDENTIFIER
// -2 IF TABLE FULL
// -3 IF IDENTIFIER ALREADY EXISTS AND IS NOT OF TYPE 'UNARY' OR 'BINARY'
// -4 IF IDENTIFIER ALREADY EXISTS AND IS A KEYWORD (IF NOT IN SETUP MODE)

// NOTE: THE UNARY AND BINARY VERSIONS OF ONE OPERATOR ARE EITHER BOTH
// KEYWORDS OR BOTH NORMAL OPS. (IT DEPENDS ON THE FIRST REGISTERED VERSION)
{
 char kw=Set_Up;
 IDENTIFIER * i;

 if(NOT Is_Identifier(identifier)) return(-1);

 if(Defined(identifier)>=0) {
  if(Var(identifier)) return(-3);
  if((i=Binary(identifier))!=NULL) kw=i->keyword;	// get binary op type
  if((i=Unary(identifier))!=NULL) {
   if(i->keyword AND NOT Set_Up) return(-4);
   i->precedence=precedence;
   i->unary_op=un_op;			// redefine unary operator
   return(0);
  }
 }

 if(identifiers==allocated) return(-2);
 if(kw AND NOT Set_Up) return(-4);		// the binary one is a keyword
 set(identT[identifiers].name,identifier);	// add an unary operator
 identT[identifiers].type=UNARY;
 identT[identifiers].keyword=kw;		// same as possibly existing
 identT[identifiers].precedence=precedence;	// binary op
 identT[identifiers].unary_op=un_op;
 identifiers++;
 Pelements=0;
 return(0);
}

int EXPRESSION::Set(_conststring_ identifier,double (* bin_op)(double,double,EVAL_ERROR *),char precedence)
// ADDS OR REDEFINES A BINARY OPERATOR WITH A GIVEN EVALUATION PRECEDENCE.
// PRECEDENCE MUST BE A NUMBER IN THE RANGE 0-31, WHERE HIGHER VALUES MEAN
// LOWER PRECEDENCE.
// RETURNS 0 ON SUCCESS
// -1 IF INVALID IDENTIFIER
// -2 IF TABLE FULL
// -3 IF IDENTIFIER ALREADY EXISTS AND IS NOT OF TYPE 'UNARY' OR 'BINARY'
// -4 IF IDENTIFIER ALREADY EXISTS AND IS A KEYWORD (IF NOT IN SETUP MODE)
{
 char kw=Set_Up;
 IDENTIFIER * i;

 if(NOT Is_Identifier(identifier)) return(-1);

 if(Defined(identifier)>=0) {
  if(Var(identifier)) return(-3);
  if((i=Unary(identifier))!=NULL) kw=i->keyword;	// get unary op type
  if((i=Binary(identifier))!=NULL) {
   if(i->keyword AND NOT Set_Up) return(-4);
   i->precedence=precedence;
   i->binary_op=bin_op;		// redefine binary operator
   return(0);
  }
 }

 if(identifiers==allocated) return(-2);
 if(kw AND NOT Set_Up) return(-4);		// the binary one is a keyword
 set(identT[identifiers].name,identifier);	// add a binary operator
 identT[identifiers].type=BINARY;
 identT[identifiers].keyword=kw;		// same as possibly existing
 identT[identifiers].precedence=precedence;	// unary op
 identT[identifiers].binary_op=bin_op;
 identifiers++;
 Pelements=0;
 return(0);
}

int EXPRESSION::GetIndex(_conststring_ identifier)
// GETS AN INDEX OF A VARIABLE IN THE IDENTIFIER TABLE. THIS INDEX IS
// THEN USED AS A PARAMETER OF SetValue MEMBER FUNCTION
// RETURNS index ON SUCCESS
// -1 IF INVALID IDENTIFIER
// -3 IF IDENTIFIER DOES NOT EXIST OR IS NOT OF TYPE 'VAR'
// NOTE: YOU CAN GET AN INDEX OF A KEYWORD AND THEN CHANGE ITS VALUE
// EVEN IN NORMAL MODE, BECAUSE SetValue DOES NOT TEST ANYTHING TO ACHIEVE
// HIGHEST SPEED IN LOOPS.
{
 if(NOT Is_Identifier(identifier)) return(-1);

 if(NOT Var(identifier)) return(-3);
 return(Defined(identifier));
}

void EXPRESSION::SetValue(int index,double value)
// REDEFINES A VARIABLE WITH AN INDEX OBTAINED BY GetIndex MEMBER FUNCTION.
// THIS METHOD IS FASTER THAN TO USE THE STANDARD Set MEMBER FUNCTION TO
// REDEFINE A VARIABLE. THIS FUNCTION DOES NOT CHECK WHETHER index IS
// VALID!!!
{
 identT[index].value=value;
}

int EXPRESSION::Remove(_conststring_ identifier)
// UNDEFINES AN IDENTIFIER
// RETURNS 0 ON SUCCESS
// -1 IF INVALID IDENTIFIER
// -2 IF IDENTIFIER NOT DEFINED
// -4 IF IDENTIFIER IS A KEYWORD (IF NOT IN SETUP MODE)
{
 int q;
 TYPE_ID t;

 if(NOT Is_Identifier(identifier)) return(-1);
 if((q=Defined(identifier))<0) return(-2);

 if(identT[q].keyword AND NOT Set_Up) return(-4);

 t=identT[q].type;
 identifiers--;
 identT[q]=identT[identifiers];
 // try to remove all (both) identifiers (unary and binary version):
 // The maximum number of recursive calls is 2, and after the second call,
 // the Remove function will find out there is no such identifier and will
 // return -2. Both binary and unary operator versions are either keywords
 // or normal identifiers.
 if(t==UNARY OR t==BINARY) Remove(identifier);
 Pelements=0;
 return(0);
}
// --------------------------------
// THE EVALUATION ALGORITHM ITSELF:

int EXPRESSION::Parse(_conststring_ expr)
// PERFORMS LEXICAL ANALYSIS ON THE GIVEN STRING AND PREPARES THE
// EXPRESSION FOR (POSSIBLY MULTIPLE) EVALUATION IN THE INTERNAL BUFFER.

// NOTE: THE PARSING PHASE MUST BE RE-DONE EVERY TIME AN IDENTIFIER IS
// ADDED OR REMOVED. YOU DON'T HAVE TO RE-PARSE THE EXPRESSION AFTER YOU
// R E D E F I N E AN IDENTIFIER! (USEFUL FOR DRAWING FUNCTION GRAPHS etc.)

// THIS FUNCTION CAN GENERATE E_SYNTAX ERROR. IN THIS CASE, PreLoad BUFFER
// IS FLUSHED AND A CALL TO Eval TRIGGERS THE E_PRELOAD ERROR.

// RETURNS 0 ON SUCCESS, -1 ON ERROR
{
 Location=0; Pelements=0;
 int q,l=len(expr);
 char c;
 int parenths=0;
 char buf[34];			// one character as reservoir
 char stg_buf[2];		// buffer used to assemble one-character strings
				// (see the _stg macro in strings.h)
 // (length check is performed after assignment)
 TYPE_ID lastType=BINARY;
 IDENTIFIER * i;

 while(Location<l) {
  if(Pelements==Psize) { Pelements=0; Error=E_PRELOAD; return(-1); } // preload table full
  Error=NO_ERROR;
  set(buf,_NOSTRING);				// new lexical element
  q=0;

  for(;;) {
   if(Location>=l) break;			// end of expression
   c=expr[Location++];
   if(Is_Space(c)) break;			// end of lexical element
   if(Is_Special(c) OR c=='(' OR c==')')
    if(q==0) {
     add(buf,_stg(c,stg_buf));			// special character identifier or parenthesis
     q++; break;
    } else
     if(NOT Is_Identifier(buf) AND tolower(buf[q-1])=='e' AND (c=='+' OR c=='-')) {
      add(buf,_stg(c,stg_buf));			// exponent sign in number
     } else {
      Location--; break;				// terminate current identifier
     }
   if(Is_Alpha(c) OR Is_Digit(c)) add(buf,_stg(c,stg_buf));
   q++;
   if(q>32) { Pelements=0; Error=E_SYNTAX; return(-1); } // element too long
  } // ends FOR
  if(len(buf)==0)
   if(Location>=l) break;	// end of expression (last element already processed)
   else continue;		// single space encountered

 // ------------ lexical element extracted in buf now ------------

  if(Is_Identifier(buf)) {
   if(Defined(buf)<0)
    { Error=E_SYNTAX; Pelements=0; return(-1); }	// undefined symbol

   if((i=Binary(buf))!=NULL) {
    switch(lastType) {	// disallowed preceding types
     case L_PAR: case BINARY: case UNARY:
     if((i=Unary(buf))!=NULL) goto unary;
     Error=E_SYNTAX; Pelements=0; return(-1);
    }
    lastType=BINARY;
   } else if((i=Unary(buf))!=0) {
    unary:
    if(i->precedence<0) {		// postfix version
     switch(lastType) {	// disallowed preceding types for postfix version
      case L_PAR: case BINARY: case UNARY:
      Error=E_SYNTAX; Pelements=0; return(-1);
     }
     lastType=POSTFIX;
    } else {				// prefix version
     switch(lastType) {	// disallowed preceding types
      case POSTFIX: case NUMBER: case R_PAR: case VAR:
      Error=E_SYNTAX; Pelements=0; return(-1);
     }
     lastType=UNARY;			// this means the prefix version
    }					// (prefix versions can accumulate
   } else if((i=Var(buf))!=NULL) {	// on the stack, but postfix ones can't)
    switch(lastType) {	// disallowed preceding types
     case POSTFIX: case NUMBER: case R_PAR: case VAR:
     Error=E_SYNTAX; Pelements=0; return(-1);
    }
    lastType=VAR;
   }

   PreLoad[Pelements].ident=i;
  } else { // relates to if(Is_Identifier...

   if(match(buf,"(")) {
    switch(lastType) {	// disallowed preceding types
     case POSTFIX: case NUMBER: case R_PAR: case VAR:
     Error=E_SYNTAX; Pelements=0; return(-1);
    }
    parenths++;
    lastType=L_PAR;
   } else if(match(buf,")")) {
    switch(lastType) {	// disallowed preceding types
     case BINARY: case UNARY: case L_PAR:
     Error=E_SYNTAX; Pelements=0; return(-1);
    }
    parenths--;
    if(parenths<0) { Error=E_SYNTAX; Pelements=0; return(-1); }
    lastType=R_PAR;
   } else {				// everything other is a number
    switch(lastType) {	// disallowed preceding types
     case POSTFIX: case NUMBER: case R_PAR: case VAR:
     Error=E_SYNTAX; Pelements=0; return(-1);
    }
    lastType=NUMBER;
    PreLoad[Pelements].value=float_val(buf);
   }

  } // ends if(Is_Identifier...
  PreLoad[Pelements].type=lastType;
  PreLoad[Pelements].loc=Location;
  Pelements++;
 } // ends WHILE

 // last element must be a number or right parenthesis
 switch(lastType) {
  case UNARY: case BINARY: case L_PAR:
  Error=E_SYNTAX; Pelements=0; return(-1);
 }
 return(0);
}

double EXPRESSION::Eval()
// EVALUATES THE ALREADY PRE-PARSED EXPRESSION. MAY RETURN ALL TYPES
// OF ERRORS EXCEPT OF E_SYNTAX
{
 double value;
 IDENTIFIER * i;
 TYPE_ID t;

 Vptr=0; Optr=0;		// reset stacks
 Location=0;
 unsigned ptr=0;

 this_e=this;			// set up the pointer to this evaluator instance

 if(Pelements==0) { Error=E_PRELOAD; return(0); }

 while(ptr<Pelements) {
  Error=NO_ERROR;
  Location=PreLoad[ptr].loc;
  t=PreLoad[ptr].type;

  if(t<4) {			// defined identifier
   i=PreLoad[ptr].ident;

   if(t==BINARY) {
    Eval_Stack(i->precedence); if(Error) return(0);
    if(Optr==OPSTACK_SIZE) { Error=E_STACK; return(0); }
    OpStack[Optr].type=BINARY;
    OpStack[Optr].precedence=i->precedence;
    OpStack[Optr].binary_op=i->binary_op;
    Optr++;
   } else if(t==POSTFIX) {
    Eval_Stack(-i->precedence); if(Error) return(0);
    // immediately run the postfix operator on the top value in value stack
    VStack[Vptr-1]=i->unary_op(VStack[Vptr-1],&Error);
    if(Error) return(0);
   } else if(t==UNARY) {
    if(Optr==OPSTACK_SIZE) { Error=E_STACK; return(0); }
    OpStack[Optr].type=UNARY;
    OpStack[Optr].precedence=i->precedence;
    OpStack[Optr].unary_op=i->unary_op;
    Optr++;
   } else {
    value=i->value;
    goto number;
   }
  } else { // relates to if(t<4...
   if(t==L_PAR) {
    if(Optr==OPSTACK_SIZE) { Error=E_STACK; return(0); }
    OpStack[Optr].type=L_PAR;
    Optr++;
    ptr++; continue;
   } else if(t==R_PAR) {
    Eval_Stack(32); if(Error) return(0);
    ptr++; continue;
   }

   // everything other is a number:
   value=PreLoad[ptr].value;
   number:
   if(Vptr==VSTACK_SIZE) { Error=E_STACK; return(0); }
   VStack[Vptr]=value;
   Vptr++;
  } // ends if(t<4...
  ptr++;
 } // ends WHILE

 // evaluate rest of the stacks:
 Eval_Stack(33); if(Error) return(0);
 return(*VStack);
}

double EXPRESSION::operator()(_conststring_ expr)
// PARSES AND EVALUATES THE GIVEN EXPRESSION
{
 if(Parse(expr)) return(0);
 else return(Eval());
}
