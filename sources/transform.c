/** @file transform.c
 * 
 *	Routines that deal with the transform statement and its varieties.
 */
/* #[ License : */
/*
 *   Copyright (C) 1984-2013 J.A.M. Vermaseren
 *   When using this file you are requested to refer to the publication
 *   J.A.M.Vermaseren "New features of FORM" math-ph/0010025
 *   This is considered a matter of courtesy as the development was paid
 *   for by FOM the Dutch physics granting agency and we would like to
 *   be able to track its scientific use to convince FOM of its value
 *   for the community.
 *
 *   This file is part of FORM.
 *
 *   FORM is free software: you can redistribute it and/or modify it under the
 *   terms of the GNU General Public License as published by the Free Software
 *   Foundation, either version 3 of the License, or (at your option) any later
 *   version.
 *
 *   FORM is distributed in the hope that it will be useful, but WITHOUT ANY
 *   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *   details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with FORM.  If not, see <http://www.gnu.org/licenses/>.
 */
/* #] License : */ 
/*
  	#[ Includes : transform.c
*/

#include "form3.h"

/*
  	#] Includes : 
 	#[ Transform :
 		#[ Intro :

		Here are the routines for the transform statement. This is a 
		group of transformations on function arguments or groups of
		function arguments. The purpose of this command is that it
		avoids repetitive pattern matching.
		Syntax:
			Transform,SetOfFunctions,OneOrMoreTransformations;
		Each transformation is given by
			Replace(argfirst,arglast)=(,,,)
			Encode(argfirst,arglast):base=#
			Decode(argfirst,arglast):base=#
			Implode(argfirst,arglast)
			Explode(argfirst,arglast)
			Permute(cycle)(cycle)(cycle)...(cycle)
			Reverse(argfirst,arglast)
			Cycle(argfirst,arglast)=+/-num
			IsLyndon(argfirst,arglast)=(yes,no)
			ToLyndon(argfirst,arglast)=(yes,no)
		In replace the extra information is
			a replace_() without the name of the replace_ function.
			This can be as in (0,1,1,0) or (xarg_,1-xarg_) to indicate
			a symbolic argument or (x,y,y,x) to exchange x and y, etc.
		In Encode and Decode argfirst is the most significant 'word' and
		arglast is the least significant 'word'.
		Note that we need to introduce the generic symbolic arguments xarg_,
		parg_, iarg_ and farg_.
		Examples:
			Transform,{H,E}
					,Replace(1:`WEIGHT')=(0,1,1,0)
					,Encode(1:`WEIGHT')=base(2);
			Transform,{H,E}
					,Decode(1:`WEIGHT')=base(3)
					,Replace(1:`WEIGHT')=(2,-1,1,0,0,1);
		Others that can be added:
			symmetrize?

 		#] Intro : 
 		#[ CoTransform :
*/

static WORD tranarray[10] = { SUBEXPRESSION, SUBEXPSIZE, 0, 1, 0, 0, 0, 0, 0, 0 };

int CoTransform(UBYTE *in)
{
	GETIDENTITY
	UBYTE *s = in, c, *ss, *Tempbuf;
	WORD number, type, num, i, *work = AT.WorkPointer+2, *wp, range[2], one = 1;
	WORD numdol, *wstart;
	int error = 0, irhs;
	LONG x;
	while ( *in == ',' ) in++;
	num = 0; wp = work + 1;
/*
  	#[ Sets :

	First the set specification(s). No sets means all functions (dangerous!)
*/
	for(;;) {
		if ( *in == '{' ) {
			s = in+1;
			SKIPBRA2(in)
			number = DoTempSet(s,in);
			in++;
			if ( *in != ',' ) {
				c = in[1]; in[1] = 0;
				MesPrint("& %s: A set in a transform statement should be followed by a comma",s);
				in[1] = c; in++;
				if ( error == 0 ) error = 1;
			}
		}
		else if ( *in == '[' || FG.cTable[*in] == 0 ) {
			s = in;
			in = SkipAName(in);
			if ( *in != ',' ) break;
			c = *in; *in = 0;
		    type = GetName(AC.varnames,s,&number,NOAUTO);
			if ( type == CFUNCTION ) { number += MAXVARIABLES + FUNCTION; }
			else if ( type != CSET ) {
				MesPrint("& %s: A transform statement starts with sets of functions",s);
				if ( error == 0 ) error = 1;
			}
			*in++ = c;
		}
		else {
			MesPrint("&Illegal syntax in Transform statement",s);
			if ( error == 0 ) error = 1;
			return(error);
		}
		if ( number >= 0 ) {
		  if ( number < MAXVARIABLES ) {
/*
			Check that this is a set of functions
*/
			if ( Sets[number].type != CFUNCTION ) {
				MesPrint("&A set in a transform statement should be a set of functions");
				if ( error == 0 ) error = 1;
			}
		  }
		}
		else if ( error == 0 ) error = 1;
/*
		Now write the number to the right place
*/
		*wp++ = number;
		num++;
		while ( *in == ',' ) in++;
	}
	*work = wp - work;
	work = wp; wp++;
/*
  	#] Sets : 

	Now we should loop over the various transformations
*/
	while ( *s ) {
		in = s;
		if ( FG.cTable[*in] != 0 ) {
			MesPrint("&Illegal character in Transform statement");
			if ( error == 0 ) error = 1;
			return(error);
		}
		in = SkipAName(in);
		if ( *in == '>' || *in == '<' ) in++;
		ss = in;
		c = *ss; *ss = 0;
		if ( c != '(' ) {
			MesPrint("&Illegal syntax in specifying a transformation inside a Transform statement");
			if ( error == 0 ) error = 1;
			return(error);
		}
/*
 		#[ replace :
*/
		if ( StrICmp(s,(UBYTE *)"replace") == 0 ) {
/*
				Subkeys: (,,,) as in replace_(,,,)
			The idea here is to read the subkeys as the argument
			of a replace_ function.
			We put the whole together as in the multiply statement (which
			could just be a replace_(....)) and compile it.
			Then we expand the tree with Generator and check the complete
			expression for legality.
*/
			type = REPLACEARG;
doreplace:
			*ss = c;
			if ( ( in = ReadRange(in,range,0) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			in++;
/*
			We have replace(#,#)=(...), and we want dum_(...)  (DUMFUN)
			to send to the compiler. The pointer is after the '=';
*/
			s = in;
			if ( *s != '(' ) {
				MesPrint("&");
				if ( error == 0 ) error = 1;
				return(error);
			}
			SKIPBRA3(in);
			if ( *in != ')' ) {
				MesPrint("&");
				if ( error == 0 ) error = 1;
				return(error);
			}
			in++;
			if ( *in != ',' && *in != '\0' ) {
				MesPrint("&");
				if ( error == 0 ) error = 1;
				return(error);
			}
			i = in - s;
			ss = Tempbuf = (UBYTE *)Malloc1(i+5,"CoTransform/replace");
			*ss++ = 'd'; *ss++ = 'u'; *ss++ = 'm'; *ss++ = '_';
			NCOPY(ss,s,i)
			*ss++ = 0;
			AC.ProtoType = tranarray;
			tranarray[4] = AC.cbufnum;
			irhs = CompileAlgebra(Tempbuf,RHSIDE,AC.ProtoType);
			M_free(Tempbuf,"CoTransform/replace");
			if ( irhs < 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			tranarray[2] = irhs;
/*
			The result of the compilation goes through Generator during
			execution, because that takes care of $-variables.
			This is why we could not use replace_ and had to use dum_.
*/
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
			*wp++ = SUBEXPSIZE+4;
			for ( i = 0; i < SUBEXPSIZE; i++ ) *wp++ = tranarray[i];
			*wp++ = 1;
			*wp++ = 1;
			*wp++ = 3;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] replace : 
 		#[ encode/decode :
*/
		else if ( StrICmp(s,(UBYTE *)"decode" ) == 0 ) {
			type = DECODEARG;
			goto doencode;
		}
		else if ( StrICmp(s,(UBYTE *)"encode" ) == 0 ) {
			type = ENCODEARG;
doencode:	*ss = c;
			if ( ( in = ReadRange(in,range,2) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			in++;
			s = in; while ( FG.cTable[*in] == 0 ) in++;
			c = *in; *in = 0;
/*
				Subkeys: base=# or base=$var
*/
			if ( StrICmp(s,(UBYTE *)"base") == 0 ) {
				*in = c;
				if ( *in != '=' ) {
					MesPrint("&Illegal base specification in encode/decode transformation");
					if ( error == 0 ) error = 1;
					return(error);
				}
				in++;
				if ( *in == '$' ) {
					in++; ss = in;
					in = SkipAName(in);
					c = *in; *in = 0;
					if ( GetName(AC.dollarnames,ss,&numdol,NOAUTO) != CDOLLAR ) {
						MesPrint("&%s is undefined",ss-1);
						numdol = AddDollar(ss,DOLINDEX,&one,1);
						return(1);
					}
					*in = c;
					x = -numdol;
				}
				else {
					x = 0;
					while ( FG.cTable[*in] == 1 ) {
						x = 10*x + *in++ - '0';
						if ( x > MAXPOSITIVE2 ) {
illsize:					MesPrint("&Illegal value for base in encode/decode transformation");
							if ( error == 0 ) error = 1;
							return(error);
						}
					}
					if ( x <= 1 ) goto illsize;
				}
				if ( *in != ',' && *in != '\0' ) {
					MesPrint("&Illegal termination of transformation");
					if ( error == 0 ) error = 1;
					return(error);
				}
			}
			else {
				MesPrint("&Illegal option in encode/decode transformation");
				if ( error == 0 ) error = 1;
				return(error);
			}
/*
			Now we can put the whole statement together
			We have the set(s) in work up to wp and the range in range.
			The base is in x and the type tells whether it is encode or decode.
*/
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
			*wp++ = 4;
			*wp++ = BASECODE;
			*wp++ = (WORD)x;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] encode/decode : 
 		#[ implode :
*/
		else if ( StrICmp(s,(UBYTE *)"implode") == 0
			   || StrICmp(s,(UBYTE *)"tosumnotation") == 0 ) {
/*
				Subkeys: ?
*/
			type = IMPLODEARG;
			*ss = c;
			if ( ( in = ReadRange(in,range,1) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] implode : 
 		#[ explode :
*/
		else if ( StrICmp(s,(UBYTE *)"explode") == 0
			   || StrICmp(s,(UBYTE *)"tointegralnotation") == 0 ) {
/*
				Subkeys: ?
*/
			type = EXPLODEARG;
			*ss = c;
			if ( ( in = ReadRange(in,range,1) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] explode : 
 		#[ permute :
*/
		else if ( StrICmp(s,(UBYTE *)"permute") == 0 ) {
			type = PERMUTEARG;
			*ss = c;
			*wp++ = ARGRANGE;
			*wp++ = 1;
			*wp++ = MAXPOSITIVE2;
			*wp++ = type;
/*
			Now a sequence of cycles
*/
			do {
			  wstart = wp; wp++;
			  do {
				x = 0; in++;
				while ( FG.cTable[*in] == 1 ) {
					x = 10*x + *in++ - '0';
					if ( x > MAXPOSITIVE2 ) {
						MesPrint("&value in permute transformation too large");
						if ( error == 0 ) error = 1;
						return(error);
					}
				}
				if ( x == 0 ) {
					MesPrint("&value 0 in permute transformation not allowed");
					if ( error == 0 ) error = 1;
					return(error);
				}
				*wp++ = (WORD)x-1;
			  } while ( *in == ',' );
			  if ( *in != ')' ) {
				MesPrint("&Illegal syntax in permute transformation");
				if ( error == 0 ) error = 1;
				return(error);
			  }
			  in++;
			  if ( *in != ',' && *in != '(' && *in != '\0' ) {
				MesPrint("&Illegal ending in permute transformation");
				if ( error == 0 ) error = 1;
				return(error);
			  }
			  *wstart = wp-wstart;
			  if ( *wstart == 1 ) wstart--;
			} while ( *in == '(' );
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] permute : 
 		#[ reverse :
*/
		else if ( StrICmp(s,(UBYTE *)"reverse") == 0 ) {
			type = REVERSEARG;
			*ss = c;
			if ( ( in = ReadRange(in,range,1) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] reverse : 
 		#[ cycle :
*/
		else if ( StrICmp(s,(UBYTE *)"cycle") == 0 ) {
			type = CYCLEARG;
			*ss = c;
			if ( ( in = ReadRange(in,range,0) ) == 0 ) {
				if ( error == 0 ) error = 1;
				return(error);
			}
			*wp++ = ARGRANGE;
			*wp++ = range[0];
			*wp++ = range[1];
			*wp++ = type;
/*
			Now a sequence of cycles
*/
			in++;
			if ( *in == '+' ) {
			}
			else if ( *in == '-' ) {
				one = -1;
			}
			else {
				MesPrint("&Cycle in a Transform statement should be followed by =+/-number");
				if ( error == 0 ) error = 1;
				return(error);
			}
			in++; x = 0;
			while ( FG.cTable[*in] == 1 ) {
				x = 10*x + *in++ - '0';
				if ( x > MAXPOSITIVE2 ) {
					MesPrint("&Number in cycle in a Transform statement too big");
					if ( error == 0 ) error = 1;
					return(error);
				}
			}
			*wp++ = x*one;
			*work = wp-work;
			work = wp; *wp++ = 0;
			s = in;
		}
/*
 		#] cycle : 
 		#[ islyndon/tolyndon :
*/
		else if ( StrICmp(s,(UBYTE *)"islyndon" ) == 0 ) {
			type = ISLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"islyndon<" ) == 0 ) {
			type = ISLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"islyndon+" ) == 0 ) {
			type = ISLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"islyndon>" ) == 0 ) {
			type = ISLYNDONR;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"islyndon-" ) == 0 ) {
			type = ISLYNDONR;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"tolyndon" ) == 0 ) {
			type = TOLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"tolyndon<" ) == 0 ) {
			type = TOLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"tolyndon+" ) == 0 ) {
			type = TOLYNDON;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"tolyndon>" ) == 0 ) {
			type = TOLYNDONR;
			goto doreplace;
		}
		else if ( StrICmp(s,(UBYTE *)"tolyndon-" ) == 0 ) {
			type = TOLYNDONR;
			goto doreplace;
		}
/*
 		#] islyndon/tolyndon : 
*/
		else {
			MesPrint("&Unknown transformation inside a Transform statement: %s",s);
			*ss = c;
			if ( error == 0 ) error = 1;
			return(error);
		}
		while ( *s == ',') s++;
	}
	AT.WorkPointer[0] = TYPETRANSFORM;
	AT.WorkPointer[1] = i = wp - AT.WorkPointer;
	AddNtoL(i,AT.WorkPointer);
	return(error);
}

/*
 		#] CoTransform : 
 		#[ RunTransform :

		Executes the transform statement.
		This routine hunts down the functions and sends them to the various
		action routines.
		params: size,#set1,...,#setn, transformations

*/

WORD RunTransform(PHEAD WORD *term, WORD *params)
{
	WORD *t, *tstop, *w, *m, *out, *in, *tt, retval;
	WORD *fun, *args, *info, *infoend, *onetransform, *funs, *endfun;
	WORD *thearg = 0, *iterm, *newterm, *nt, *oldwork = AT.WorkPointer;
	int i;
	out = tstop = term + *term;
	tstop -= ABS(tstop[-1]);
	in = term;
	t = term + 1;
	while ( t < tstop ) {
		endfun = onetransform = params + *params;
		funs = params + 1;
		if ( *t < FUNCTION ) {}
		else if ( funs == endfun ) {  /* we do all functions */
hit:;
			while ( in < t ) *out++ = *in++;
			tt = t + t[1]; fun = out;
			while ( in < tt ) *out++ = *in++;
			do {
				args = onetransform + 1;
				info = args; while ( *info <= MAXRANGEINDICATOR ) {
					if ( *info == ALLARGS ) info++;
					else if ( *info == NUMARG ) info += 2;
					else if ( *info == ARGRANGE ) info += 3;
					else if ( *info == MAKEARGS ) info += 3;
				}
				switch ( *info ) {
					case REPLACEARG:
						if ( RunReplace(BHEAD fun,args,info) ) goto abo;
						out = fun + fun[1];
						break;
					case ENCODEARG:
						if ( RunEncode(BHEAD fun,args,info) ) goto abo;
						out = fun + fun[1];
						break;
					case DECODEARG:
						if ( RunDecode(BHEAD fun,args,info) ) goto abo;
						out = fun + fun[1];
						break;
					case IMPLODEARG:
						if ( RunImplode(fun,args) ) goto abo;
						out = fun + fun[1];
						break;
					case EXPLODEARG:
						if ( RunExplode(BHEAD fun,args) ) goto abo;
						out = fun + fun[1];
						break;
					case PERMUTEARG:
						if ( RunPermute(BHEAD fun,args,info) ) goto abo;
						out = fun + fun[1];
						break;
					case REVERSEARG:
						if ( RunReverse(BHEAD fun,args) ) goto abo;
						out = fun + fun[1];
						break;
					case CYCLEARG:
						if ( RunCycle(BHEAD fun,args,info) ) goto abo;
						out = fun + fun[1];
						break;
					case ISLYNDON:
						if ( ( retval = RunIsLyndon(BHEAD fun,args,1) ) < -1 ) goto abo;
						goto returnvalues;
						break;
					case ISLYNDONR:
						if ( ( retval = RunIsLyndon(BHEAD fun,args,-1) ) < -1 ) goto abo;
						goto returnvalues;
						break;
					case TOLYNDON:
						if ( ( retval = RunToLyndon(BHEAD fun,args,1) ) < -1 ) goto abo;
						goto returnvalues;
						break;
					case TOLYNDONR:
						if ( ( retval = RunToLyndon(BHEAD fun,args,-1) ) < -1 ) goto abo;
returnvalues:;
						out = fun + fun[1];
						if ( retval == -1 ) break;
/*
						Work out the yes/no stuff
*/
						AT.WorkPointer += 2*AM.MaxTer;
						if ( AT.WorkPointer > AT.WorkTop ) {
							MLOCK(ErrorMessageLock);
							MesWork();
							MUNLOCK(ErrorMessageLock);
							return(-1);
						}
						iterm = AT.WorkPointer;
						info++;
						for ( i = 0; i < *info; i++ ) iterm[i] = info[i];
						AT.WorkPointer = iterm + *iterm;
						AR.Eside = LHSIDEX;
						NewSort(BHEAD0);
						if ( Generator(BHEAD iterm,AR.Cnumlhs) ) {
							LowerSortLevel();
							AT.WorkPointer = oldwork;
							return(-1);
						}
						newterm = AT.WorkPointer;
						if ( EndSort(BHEAD newterm,0) < 0 ) {}
						if ( ( *newterm && *(newterm+*newterm) != 0 ) || *newterm == 0 ) {
							MLOCK(ErrorMessageLock);
							MesPrint("&yes/no information in islyndon/tolyndon does not evaluate into a single term");
							MUNLOCK(ErrorMessageLock);
							return(-1);
						}
						AR.Eside = RHSIDE;
						i = *newterm; tt = iterm; nt = newterm;
						NCOPY(tt,nt,i);
						AT.WorkPointer = iterm + *iterm;
						info = iterm + 1;
						infoend = info+info[1];
						info += FUNHEAD;

						if ( retval == 0 ) {
/*
							Need second argument (=no)
*/
							if ( info >= infoend ) {
abortlyndon:;
								MLOCK(ErrorMessageLock);
								MesPrint("There should be a yes and a no argument in islyndon/tolyndon");
								MUNLOCK(ErrorMessageLock);
								Terminate(-1);
							}
							NEXTARG(info)
							if ( info >= infoend ) goto abortlyndon;
							thearg = info;
						}
						else if ( retval == 1 ) {
/*
							Need first argument (=yes)
*/
							if ( info >= infoend ) goto abortlyndon;
							thearg = info;
							NEXTARG(info)
							if ( info >= infoend ) goto abortlyndon;
						}
						NEXTARG(info)
						if ( info < infoend ) goto abortlyndon;
/*
						The argument in thearg needs to be copied
						We did not pull it through generator to guarantee
						that it is a single argument.
						The easiest way is to let the routine Normalize
						do the job and put everything in an exponent function
						with the power one.
*/
						if ( *thearg == -SNUMBER && thearg[1] == 0 ) {
							*term = 0; return(0);
						}
						if ( *thearg == -SNUMBER && thearg[1] == 1 ) { }
						else {
							fun = out;
							*out++ = EXPONENT; out++; *out++ = 1; FILLFUN3(out);
							COPY1ARG(out,thearg);
							*out++ = -SNUMBER; *out++ = 1;
							fun[1] = out-fun;
						}
						break;
					default:
						MLOCK(ErrorMessageLock);
						MesPrint("Irregular code in execution of transform statement");
						MUNLOCK(ErrorMessageLock);
						Terminate(-1);
				}
				onetransform += *onetransform;
			} while ( *onetransform );
		}
		else {
		  while ( funs < endfun ) {  /* sum over sets */
			if ( *funs > MAXVARIABLES ) {
				if ( *t == *funs-MAXVARIABLES ) goto hit;
			}
			else {
			  w = SetElements + Sets[*funs].first;
			  m = SetElements + Sets[*funs].last;
			  while ( w < m ) {  /* sum over set elements */
				if ( *w == *t ) goto hit;
				w++;
			  }
			}
			funs++;
		  }
		}
		t += t[1];
	}
	tt = term + *term; while ( in < tt ) *out++ = *in++;
	*tt = i = out - tt;
/*
	Now copy the whole thing back
*/
	NCOPY(term,tt,i)
	return(0);
abo:
	MLOCK(ErrorMessageLock);
	MesCall("RunTransform");
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunTransform : 
 		#[ RunEncode :

		The info is given by
			ENCODEARG,size,BASECODE,num
		and possibly more codes to follow.
		Only one range is allowed and for now, it should be fully numerical
		If the range is in reverse order, we need to either revert it
		first or work with an array of pointers.
*/

WORD RunEncode(PHEAD WORD *fun, WORD *args, WORD *info)
{
	WORD base, *f, *funstop, *fun1, *t, size1, size2, size3, *arg;
	int num, num1, num2, n, i, i1, i2;
	UWORD *scrat1, *scrat2, *scrat3;
	WORD *tt, *tstop, totarg, arg1, arg2;
	if ( functions[fun[0]-FUNCTION].spec != 0 ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunEncode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	if ( arg1 > totarg || arg2 > totarg ) return(0);

	if ( info[2] == BASECODE ) {
		base = info[3];
		if ( base <= 0 ) { /* is a dollar variable */
			i1 = -base;
			base = DolToNumber(BHEAD i1);
			if ( AN.ErrorInDollar || base < 2 ) {
				MLOCK(ErrorMessageLock);
				MesPrint("$%s does not have a number value > 1 in base/encode/transform statement in module %l",
					DOLLARNAME(Dollars,i1),AC.CModule);
				MUNLOCK(ErrorMessageLock);
				Terminate(-1);
			}
		}
/*
		Compute number of pointers needed and make sure there is space
*/
		if ( arg1 > arg2 ) { num1 = arg2; num2 = arg1; }
		else { num1 = arg1; num2 = arg2; }
		num = num2-num1+1;
		WantAddPointers(num);
/*
		Collect the pointers in pWorkSpace
*/
		n = 1; funstop = fun+fun[1]; f = fun+FUNHEAD;
		while ( n < num1 ) {
			if ( f >= funstop ) return(0);
			NEXTARG(f);
			n++;
		}
		fun1 = f; i = 0;
		while ( n <= num2 ) {
			if ( f >= funstop ) return(0);
			if ( *f != -SNUMBER ) {
				if ( *f < 0 ) return(0);
				t = f + *f - 1;
				i1 = ABS(*t);
				if ( (*f-i1) != (ARGHEAD+1) ) return(0); /* Not numerical */
				i1 = (i1-1)/2 - 1;
				t--;
				while ( i1 > 0 ) {
					if ( *t != 0 ) return(0); /* Not an integer */
					t--; i1--;
				}
			}
			AT.pWorkSpace[AT.pWorkPointer+i] = f;
			i++;
			NEXTARG(f);
			n++;
		}
/*
		f points now to after the arguments; fun1 at the first.
		Now check whether we need to revert the order
*/
		if ( arg1 > arg2 ) {
			i1 = 0; i2 = i-1;
			while ( i1 < i2 ) {
				t = AT.pWorkSpace[AT.pWorkPointer+i1];
				AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
				AT.pWorkSpace[AT.pWorkPointer+i2] = t;
				i1++; i2--;
			}
		}
/*
		Now we can put the thing together.
		x = arg1;
		x = base*x+arg2
		x = base*x+arg3  etc.
		We need three scratch arrays for long integers
				(see NumberMalloc in tools.c).
*/
		scrat1 = NumberMalloc("RunEncode");
		scrat2 = NumberMalloc("RunEncode");
		scrat3 = NumberMalloc("RunEncode");
		arg = AT.pWorkSpace[AT.pWorkPointer];
		size1 = PutArgInScratch(arg,scrat1);
		i--;
		while ( i > 0 ) {
			if ( MulLong(scrat1,size1,(UWORD *)(&base),1,scrat2,&size2) ) {
				NumberFree(scrat3,"RunEncode");
				NumberFree(scrat2,"RunEncode");
				NumberFree(scrat1,"RunEncode");
				goto CalledFrom;
			}
			NEXTARG(arg);
			size3 = PutArgInScratch(arg,scrat3);
			if ( AddLong(scrat2,size2,scrat3,size3,scrat1,&size1) ) {
				NumberFree(scrat3,"RunEncode");
				NumberFree(scrat2,"RunEncode");
				NumberFree(scrat1,"RunEncode");
				goto CalledFrom;
			}
			i--;
		}
/*
		Now put the output in place. There are two cases, one being much
		faster than the other. Hence we program both.
		Fast: it fits inside the old location.
		Slow: it does not.
		The total space is f-fun1
*/
		if ( size1 == 0 ) {	/* Fits! */
			*fun1++ = -SNUMBER; *fun1++ = 0;
			while ( f < funstop ) *fun1++ = *f++;
			fun[1] = funstop-fun;
		}
		else if ( size1 == 1 && scrat1[0] <= MAXPOSITIVE ) { /* Fits! */
			*fun1++ = -SNUMBER; *fun1++ = scrat1[0];
			while ( f < funstop ) *fun1++ = *f++;
			fun[1] = fun1-fun;
		}
		else if ( size1 == -1 && scrat1[0] <= MAXPOSITIVE+1 ) { /* Fits! */
			*fun1++ = -SNUMBER;
			if ( scrat1[0] < MAXPOSITIVE ) *fun1++ = scrat1[0];
			else *fun1++ = (WORD)(MAXPOSITIVE+1);
			while ( f < funstop ) *fun1++ = *f++;
			fun[1] = fun1-fun;
		}
		else if ( ABS(size1)*2+2+ARGHEAD <= f-fun1 ) { /* Fits! */
			if ( size1 < 0 ) { size2 = size1*2-1; size1 = -size1; size3 = -size2; }
			else { size2 = 2*size1+1; size3 = size2; }
			*fun1++ = size3+ARGHEAD+1;
			*fun1++ = 0; FILLARG(fun1);
			*fun1++ = size3+1;
			for ( i = 0; i < size1; i++ ) *fun1++ = scrat1[i];
			*fun1++ = 1;
			for ( i = 1; i < size1; i++ ) *fun1++ = 0;
			*fun1++ = size2;
			while ( f < funstop ) *fun1++ = *f++;
			fun[1] = fun1-fun;
		}
		else {	/* Does not fit */
			t = funstop;
			if ( size1 < 0 ) { size2 = size1*2-1; size1 = -size1; size3 = -size2; }
			else { size2 = 2*size1+1; size3 = size2; }
			*t++ = size3+ARGHEAD+1;
			*t++ = 0; FILLARG(t);
			*t++ = size3+1;
			for ( i = 0; i < size1; i++ ) *t++ = scrat1[i];
			*t++ = 1;
			for ( i = 1; i < size1; i++ ) *t++ = 0;
			*t++ = size2;
			while ( f < funstop ) *t++ = *f++;
			f = funstop;
			while ( f < t ) *fun1++ = *f++;
			fun[1] = fun1-fun;
		}
		NumberFree(scrat3,"RunEncode");
		NumberFree(scrat2,"RunEncode");
		NumberFree(scrat1,"RunEncode");
	}
	else {
		MLOCK(ErrorMessageLock);
		MesPrint("Unimplemented type of encoding encountered in RunEncode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	return(0);
CalledFrom:
	MLOCK(ErrorMessageLock);
	MesCall("RunEncode");
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunEncode : 
 		#[ RunDecode :
*/

WORD RunDecode(PHEAD WORD *fun, WORD *args, WORD *info)
{
	WORD base, num, num1, num2, n, *f, *funstop, *fun1, size1, size2, size3, *t;
	WORD i1, i2, i, sig;
	UWORD *scrat1, *scrat2, *scrat3;
	WORD *tt, *tstop, totarg, arg1, arg2;
	if ( functions[fun[0]-FUNCTION].spec != 0 ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunDecode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	if ( arg1 > totarg && arg2 > totarg ) return(0);
	if ( info[2] == BASECODE ) {
		base = info[3];
		if ( base <= 0 ) { /* is a dollar variable */
			i1 = -base;
			base = DolToNumber(BHEAD i1);
			if ( AN.ErrorInDollar || base < 2 ) {
				MLOCK(ErrorMessageLock);
				MesPrint("$%s does not have a number value > 1 in base/decode/transform statement in module %l",
					DOLLARNAME(Dollars,i1),AC.CModule);
				MUNLOCK(ErrorMessageLock);
				Terminate(-1);
			}
		}
/*
		Compute number of output arguments needed
*/
		if ( arg1 > arg2 ) { num1 = arg2; num2 = arg1; }
		else { num1 = arg1; num2 = arg2; }
		num = num2-num1+1;
		if ( num <= 1 ) return(0);
/*
		Find argument num1
*/
		funstop = fun + fun[1];
		f = fun + FUNHEAD; n = 1;
		while ( f < funstop ) {
			if ( n == num1 ) break;
			NEXTARG(f); n++;
		}
		if ( f >= funstop ) return(0);	/* not enough arguments */
/*
		Check that f is integer
*/
		if ( *f == -SNUMBER ) {}
		else if ( *f < 0 ) return(0);
		else {
			t = f + *f - 1;
			i1 = ABS(*t);
			if ( (*f-i1) != (ARGHEAD+1) ) return(0); /* Not numerical */
			i1 = (i1-1)/2 - 1;
			t--;
			while ( i1 > 0 ) {
				if ( *t != 0 ) return(0); /* Not an integer */
				t--; i1--;
			}
		}
		fun1 = f;
/*
		The argument that should be decoded is in fun1
		We have to copy it to scratch
*/
		scrat1 = NumberMalloc("RunEncode");
		scrat2 = NumberMalloc("RunEncode");
		scrat3 = NumberMalloc("RunEncode");
		size1 = PutArgInScratch(fun1,scrat1);
		if ( size1 < 0 ) { sig = -1; size1 = -size1; }
		else sig = 1;
/*
		We can check first whether this number can be decoded
*/
		scrat2[0] = base; size2 = 1;
		if ( RaisPow(BHEAD scrat2,&size2,num) ) {
			NumberFree(scrat3,"RunEncode");
			NumberFree(scrat2,"RunEncode");
			NumberFree(scrat1,"RunEncode");
			goto CalledFrom;
		}
		if ( BigLong(scrat1,size1,scrat2,size2) >= 0 ) { /* Number too big */
			NumberFree(scrat3,"RunEncode");
			NumberFree(scrat2,"RunEncode");
			NumberFree(scrat1,"RunEncode");
			return(0);
		}
/*
		We need num*2 spaces
*/
		if ( *fun1 > num*2 ) {  /* shrink space */
			t = fun1 + 2*num; f = fun1 + *fun1;
			while ( f < funstop ) *t++ = *f++;
			fun[1] = t - fun;
		}
		else if ( *fun1 < num*2 ) { /* case includes -SNUMBER */
			if ( *fun1 < 0 ) { /* expand space from -SNUMBER */
				fun[1] += (num-1)*2;
				t = funstop + (num-1)*2;
			}
			else { /* expand space from general argument */
				fun[1] += 2*num - *fun1;
				t = funstop +2*num - *fun1;
			}
			f = funstop;
			while ( f > fun1 ) *--t = *--f;
		}
/*
		Now there is space for num -SNUMBER arguments filled from the top.
*/
		for ( i = num-1; i >= 0; i-- ) {
			DivLong(scrat1,size1,(UWORD *)(&base),1,scrat2,&size2,scrat3,&size3);
			fun1[2*i]   = -SNUMBER;
			if ( size3 == 0 ) fun1[2*i+1] = 0;
			else fun1[2*i+1] = (WORD)(scrat3[0])*sig;
			for ( i1 = 0; i1 < size2; i1++ ) scrat1[i1] = scrat2[i1];
			size1 = size2;
		}
		if ( size2 != 0 ) {
			MLOCK(ErrorMessageLock);
			MesPrint("RunDecode: number to be decoded is too big");
			MUNLOCK(ErrorMessageLock);
			NumberFree(scrat3,"RunEncode");
			NumberFree(scrat2,"RunEncode");
			NumberFree(scrat1,"RunEncode");
			goto CalledFrom;
		}
/*
		Now check whether we should change the order of the arguments
*/
		if ( arg1 > arg2 ) {
			i1 = 1; i2 = 2*num-1;
			while ( i2 > i1 ) {
				i = fun1[i1]; fun1[i1] = fun1[i2]; fun1[i2] = i;
				i1 += 2; i2 -= 2;
			}
		}
		NumberFree(scrat3,"RunEncode");
		NumberFree(scrat2,"RunEncode");
		NumberFree(scrat1,"RunEncode");
	}
	else {
		MLOCK(ErrorMessageLock);
		MesPrint("Unimplemented type of encoding encountered in RunDecode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	return(0);
CalledFrom:
	MLOCK(ErrorMessageLock);
	MesCall("RunDecode");
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunDecode : 
 		#[ RunReplace :

		Gets the function, passes the arguments and looks whether they
		need to be treated. If so, the exact treatment is found in info.
		The info is given as if it is a function of type REPLACEMENT but
		its name is REPLACEARG (which is NOT a function).
		It is performed on the arguments.
		The output is at first written after fun and in the end overwrites fun.
*/

WORD RunReplace(PHEAD WORD *fun, WORD *args, WORD *info)
{
	int n = 0, i, dirty = 0, totarg, nfix, nwild, ngeneral;
	WORD *t, *tt, *u, *tstop, *info1, *infoend, *oldwork = AT.WorkPointer;
	WORD *term, *newterm, *nt, *term1, *term2;
	WORD wild[4], mask, *term3, *term4;
	WORD n1, n2;
	info++;
	t = fun; tstop = fun + fun[1]; u = tstop;
	for ( i = 0; i < FUNHEAD; i++ ) *u++ = *t++;
	tt = t;
	if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
		totarg = 0;
		while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	}
	else {
		totarg = tstop - tt;
	}
/*
	Now get the info through Generator to bring it to standard form.
	info points at a single term that should be sent to Generator.

	We want to put the information in the WorkSpace but fun etc lies there
	already. This means that we have to move the WorkPointer quite high up.
*/
	AT.WorkPointer += 2*AM.MaxTer;
	if ( AT.WorkPointer > AT.WorkTop ) {
		MLOCK(ErrorMessageLock);
		MesWork();
		MUNLOCK(ErrorMessageLock);
		return(-1);
	}
	term = AT.WorkPointer;
	for ( i = 0; i < *info; i++ ) term[i] = info[i];
	AT.WorkPointer = term + *term;
	AR.Eside = LHSIDEX;
	NewSort(BHEAD0);
	if ( Generator(BHEAD term,AR.Cnumlhs) ) {
		LowerSortLevel();
		AT.WorkPointer = oldwork;
		return(-1);
	}
	newterm = AT.WorkPointer;
	if ( EndSort(BHEAD newterm,0) < 0 ) {}
	if ( ( *newterm && *(newterm+*newterm) != 0 ) || *newterm == 0 ) {
		MLOCK(ErrorMessageLock);
		MesPrint("&information in replace transformation does not evaluate into a single term");
		MUNLOCK(ErrorMessageLock);
		return(-1);
	}
	AR.Eside = RHSIDE;
	i = *newterm; tt = term; nt = newterm;
	NCOPY(tt,nt,i);
	AT.WorkPointer = term + *term;
	info = term + 1;

	term1 = term + *term;
	term2 = term1+1;
	*term2++ = REPLACEMENT;
	term2++; FILLFUN(term2)
/*
	First we count the different types of objects
*/
	infoend = info + info[1];
	info1 = info + FUNHEAD;
	nfix = nwild = ngeneral = 0;
	while ( info1 < infoend ) {
		if ( *info1 == -SNUMBER ) {
			nfix++; 
			info1 += 2; NEXTARG(info1)
		}
		else if ( *info1 <= -FUNCTION ) {
			if ( *info1 == -WILDARGFUN ) {
				nwild++;
				info1++; NEXTARG(info1)
			}
			else {
				*term2++ = *info1++; COPY1ARG(term2,info1)
				ngeneral++;
			}
		}
		else if ( *info1 == -INDEX ) {
			if ( info1[1] == WILDARGINDEX + AM.OffsetIndex ) {
				nwild++;
				info1 += 2; NEXTARG(info1)
			}
			else {
				*term2++ = *info1++; *term2++ = *info1++; COPY1ARG(term2,info1)
				ngeneral++;
			}
		}
		else if ( *info1 == -SYMBOL ) {
			if ( info1[1] == WILDARGSYMBOL ) {
				nwild++;
				info1 += 2; NEXTARG(info1)
			}
			else {
				*term2++ = *info1++; *term2++ = *info1++; COPY1ARG(term2,info1)
				ngeneral++;
			}
		}
		else if ( *info1 == -MINVECTOR || *info1 == -VECTOR ) {
			if ( info1[1] == WILDARGVECTOR + AM.OffsetVector ) {
				nwild++;
				info1 += 2; NEXTARG(info1)
			}
			else {
				*term2++ = *info1++; *term2++ = *info1++; COPY1ARG(term2,info1)
				ngeneral++;
			}
		}
		else {
			MLOCK(ErrorMessageLock);
			MesPrint("&irregular code found in replace transformation (RunReplace)");
			MUNLOCK(ErrorMessageLock);
			Terminate(-1);
		}
	}
	AT.WorkPointer = term2;
	*term1 = term2 - term1;
	term1[2] = *term1 - 1;
/*
	And now stepping through the arguments
*/
	while ( t < tstop ) {
		n++;	/* The number of the argument. Now check whether we need it */
		if ( TestArgNum(n,totarg,args) == 0 ) {
			if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
				if ( *t <= -FUNCTION ) { *u++ = *t++; }
				else if ( *t < 0 ) { *u++ = *t++; *u++ = *t++; }
				else { i = *t; NCOPY(u,t,i) }
			}
			else *u++ = *t++;
			continue;
		}
/*
		Here we have in info effectively a replace_ function, but with
		additionally the possibility of integer arguments. We treat those first
		and for the rest we have to do some pattern matching.
		Note that the compilation routine should check that there is an
		even number of arguments in the replace function.

		First we go for number -> something
*/
		if ( nfix > 0 ) {
		  if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
			if ( *t == -SNUMBER ) {
			  info1 = info + FUNHEAD;
			  while ( info1 < infoend ) {
				if ( *info1 == -SNUMBER ) {
					if ( info1[1] == t[1] ) {
					  if ( info1[2] == -SNUMBER ) {
						*u++ = -SNUMBER; *u++ = info1[3];
						info1 += 4;
					  }
					  else {
						info1 += 2;
						if ( info1[0] <= -FUNCTION ) i = 1;
						else if ( info1[0] < 0 ) i = 2;
						else i = *info1;
						NCOPY(u,info1,i)
					  }
					  t += 2; goto nextt;
					}
					info1 += 2;
					NEXTARG(info1);
				}
				else {
					NEXTARG(info1);
					NEXTARG(info1);
				}
			  }
			}
		  }
		  else {  /* Tensor */
			if ( *t < AM.OffsetIndex && *t >= 0 ) {
			  info1 = info + FUNHEAD;
			  while ( info1 < infoend ) {
				if ( ( *info1 == -SNUMBER ) && ( info1[1] == *t )
				 && ( ( ( info1[2] == -SNUMBER ) && ( info1[3] >= 0 )
				 && ( info1[3] < AM.OffsetIndex ) )
				 || ( info1[2] == -INDEX || info1[2] == -VECTOR
				 || info1[2] == -MINVECTOR ) ) ) {
					*u++ = info1[3];
					info1 += 4;
					t++; goto nextt;
				}
				else {
					NEXTARG(info1);
					NEXTARG(info1);
				}
			  }
			}
		  }
		}
/*
		First we try to catch those elements that have an exact match
		in the traditional replace_ part.
		This means that *t should be less than zero and match an entry
		in the replace_ function that we prepared.
*/
		if ( ngeneral > 0 ) {
		  if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
			if ( *t < 0 ) {
				term3 = term1 + *term1;
				term4 = term1 + FUNHEAD;
				while ( term4 < term3 ) {
					if ( *term4 == *t && ( *t <= -FUNCTION ||
					( t[1] == term4[1] ) ) ) break;
					NEXTARG(term4)
				}
				if ( term4 < term3 ) goto dothisnow;
			}
		  }
		  else {
			term3 = term1 + *term1;
			term4 = term1 + FUNHEAD;
			while ( term4 < term3 ) {
				if ( ( term4[1] == *t ) &&
					( ( *term4 == -INDEX || *term4 == -VECTOR ||
					 ( *term4 == -SYMBOL && term4[1] < AM.OffsetIndex
						&& term4[1] >= 0 ) ) ) ) break;
				NEXTARG(term4)
			}
			if ( term4 < term3 ) goto dothisnow;
		  }
		}
/*
		First we eliminate the fixed arguments and make a 'new info'
		If there is anything left we can continue.
		Now we look for whole argument wildcards (arg_, parg_, iarg_ or farg_)
*/
		if ( nwild > 0 ) {
/*
			If we have f(a)*replace_(xarg_,b(xarg_)) this gives f(b(a))
			In testing the wildcard we have CheckWild do the work.
			This means that we have to set op the special variables
			(AT.WildMask,AN.WildValue,AN.NumWild)

*/
			wild[1] = 4;
			info1 = info + FUNHEAD;
			while ( info1 < infoend ) {
				if ( *info1 == -SYMBOL && info1[1] == WILDARGSYMBOL
				&& ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) ) {
					wild[0] = SYMTOSUB;
					wild[2] = WILDARGSYMBOL;
					wild[3] = 0;
					AN.WildValue = wild;
					AT.WildMask = &mask;
					mask = 0;
					AN.NumWild = 1;
					if ( *t == -SYMBOL || ( *t > 0 && CheckWild(BHEAD WILDARGSYMBOL,SYMTOSUB,1,t) == 0 ) ) {
/*
						We put the part in replace in a function and make
						a replace_(xarg_,(t argument)).
*/
						n1 = SYMBOL; n2 = WILDARGSYMBOL;
						info1 += 2;
getthisone:;
						term3 = term2+1;
						if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
							*term3++ = DUMFUN; term3++; FILLFUN(term3)
							COPY1ARG(term3,info1)
						}
						else {
							*term3++ = fun[0]; term3++; FILLFUN(term3)
							*term3++ = *info1;
						}
						term2[2] = term3 - term2 - 1;
						tt = term3;
						*term3++ = REPLACEMENT;
						term3++; FILLFUN(term3)
						*term3++ = -n1;
						if ( n1 < FUNCTION ) *term3++ = n2;
						if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
							term4 = t;
							COPY1ARG(term3,term4)
						}
						else {
							*term3++ = *t;
						}
						tt[1] = term3 - tt;
						*term3++ = 1; *term3++ = 1; *term3++ = 3;
						*term2 = term3 - term2;

						AT.WorkPointer = term3;
						NewSort(BHEAD0);
						if ( Generator(BHEAD term2,AR.Cnumlhs) ) {
							LowerSortLevel();
							AT.WorkPointer = oldwork;
							return(-1);
						}
						term4 = AT.WorkPointer;
						if ( EndSort(BHEAD term4,0) < 0 ) {}
						if ( ( *term4 && *(term4+*term4) != 0 ) || *term4 == 0 ) {
							MLOCK(ErrorMessageLock);
							MesPrint("&information in replace transformation does not evaluate into a single term");
							MUNLOCK(ErrorMessageLock);
							return(-1);
						}
/*
						Now we can copy the new function argument to the output u
*/
						i = term4[2]-FUNHEAD;
						term3 = term4+FUNHEAD+1;
						NCOPY(u,term3,i)
						if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
							NEXTARG(t)
						}
						else t++;
						AT.WorkPointer = term2;

						goto nextt;
					}
					info1 += 2; NEXTARG(info1)
				}
				else if ( ( *info1 == -INDEX )
					&& ( info[1] == WILDARGINDEX + AM.OffsetIndex ) ) {
					wild[0] = INDTOSUB;
					wild[2] = WILDARGINDEX+AM.OffsetIndex;
					wild[3] = 0;
					AN.WildValue = wild;
					AT.WildMask = &mask;
					mask = 0;
					AN.NumWild = 1;
					if ( ( functions[fun[0]-FUNCTION].spec == TENSORFUNCTION )
					|| ( *t == -INDEX || ( *t > 0 && CheckWild(BHEAD WILDARGINDEX,INDTOSUB,1,t) == 0 ) ) ) {
/*
						We put the part in replace in a function and make
						a replace_(xarg_,(t argument)).
*/
						n1 = INDEX; n2 = WILDARGINDEX+AM.OffsetIndex;
						info1 += 2;
						goto getthisone;
					}
					info1 += 2; NEXTARG(info1)
				}
				else if ( ( *info1 == -VECTOR )
					&& ( info1[1] == WILDARGVECTOR + AM.OffsetVector ) ) {
					wild[0] = VECTOSUB;
					wild[2] = WILDARGVECTOR+AM.OffsetVector;
					wild[3] = 0;
					AN.WildValue = wild;
					AT.WildMask = &mask;
					mask = 0;
					AN.NumWild = 1;
					if ( functions[fun[0]-FUNCTION].spec == TENSORFUNCTION ) {
						if ( *t < MINSPEC ) {
							n1 = VECTOR; n2 = WILDARGVECTOR+AM.OffsetVector;
							info1 += 2;
							goto getthisone;
						}
					}
					else if ( *t == -VECTOR || *t == -MINVECTOR ||
					( *t > 0 && CheckWild(BHEAD WILDARGVECTOR,VECTOSUB,1,t) == 0 ) ) {
/*
						We put the part in replace in a function and make
						a replace_(xarg_,(t argument)).
*/
						n1 = VECTOR; n2 = WILDARGVECTOR+AM.OffsetVector;
						info1 += 2;
						goto getthisone;
					}
					info1 += 2; NEXTARG(info1)
				}
				else if ( *info1 == -WILDARGFUN ) {
					wild[0] = FUNTOFUN;
					wild[2] = WILDARGFUN;
					wild[3] = 0;
					AN.WildValue = wild;
					AT.WildMask = &mask;
					mask = 0;
					AN.NumWild = 1;
					if ( *t <= -FUNCTION || ( *t > 0 && CheckWild(BHEAD WILDARGFUN,FUNTOFUN,1,t) == 0 ) ) {
/*
						We put the part in replace in a function and make
						a replace_(xarg_,(t argument)).
*/
						n2 = n1 = -WILDARGFUN; /* n2 is to keep the compiler quiet */
						info1++;
						goto getthisone;
					}
					info1++; NEXTARG(info1)
				}
				else {
					NEXTARG(info1) NEXTARG(info1)
				}
			}
		}
		if ( ngeneral > 0 ) {
/*
			They are all in a replace_ function.
			Compose the whole thing into a term with replace_()*dum_(arg)
			which will be given to Generator.
			If we have f(a(x))*replace_(x,b) this gives f(a(b))
*/
dothisnow:;
			term3 = term2; term4 = term1; i = *term1;
			NCOPY(term3,term4,i)
			term4 = term3;
			if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
				*term3++ = DUMFUN; term3++; FILLFUN(term3);
				tt = t;
				COPY1ARG(term3,tt)
			}
			else {
				*term3++ = fun[0]; term3++; FILLFUN(term3); *term3++ = *t;
			}
			term4[1] = term3-term4;
			*term3++ = 1; *term3++ = 1; *term3++ = 3;
			*term2 = term3-term2;
			AT.WorkPointer = term3;
			NewSort(BHEAD0);
			if ( Generator(BHEAD term2,AR.Cnumlhs) ) {
				LowerSortLevel();
				AT.WorkPointer = oldwork;
				return(-1);
			}
			term4 = AT.WorkPointer;
			if ( EndSort(BHEAD term4,0) < 0 ) {}
			if ( ( *term4 && *(term4+*term4) != 0 ) || *term4 == 0 ) {
				MLOCK(ErrorMessageLock);
				MesPrint("&information in replace transformation does not evaluate into a single term");
				MUNLOCK(ErrorMessageLock);
				return(-1);
			}
/*
			Now we can copy the new function argument to the output u
*/
			i = term4[2]-FUNHEAD;
			term3 = term4+FUNHEAD+1;
			NCOPY(u,term3,i)
			NEXTARG(t)
			AT.WorkPointer = term2;

			goto nextt;
		}

/*
		No catch. Copy the argument and continue.
*/		
		if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
			if ( *t <= -FUNCTION ) { *u++ = *t++; }
			else if ( *t < 0 ) { *u++ = *t++; *u++ = *t++; }
			else { i = *t; NCOPY(u,t,i) }
		}
		else {
			*u++ = *t++;
		}
nextt:;
	}
	i = u - tstop; tstop[1] = i; tstop[2] = dirty;
	t = fun; u = tstop; NCOPY(t,u,i)
	AT.WorkPointer = oldwork;
	return(0);
}

/*
 		#] RunReplace : 
 		#[ RunImplode :

		Note that we restrict ourselves to short integers and/or single symbols
*/

WORD RunImplode(WORD *fun, WORD *args)
{
	WORD *tt, *tstop, totarg, arg1, arg2, num1, num2, i, i1, n;
	WORD *f, *t, *ttt, *t4, *ff, *fff;
	WORD moveup, numzero, outspace;
	if ( functions[fun[0]-FUNCTION].spec != 0 ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunImplode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
/*
	Get the proper range in forward direction and the number of arguments
*/
	if ( arg1 > arg2 ) { num1 = arg2; num2 = arg1; }
	else { num1 = arg1; num2 = arg2; }
	if ( num1 > totarg || num2 > totarg ) return(0);
/*
	We need, for the most general case 4 spots for each:
			x,pow,coef,sign
	Hence we put these in the workspace above the term after tstop
*/
	n = 1; f = fun+FUNHEAD;
	while ( n < num1 ) {
		if ( f >= tstop ) return(0);
		NEXTARG(f);
		n++;
	}
	ff = f;
/*
	We are now at the first argument to be done	
	Go through the terms and test their validity.
	If one of them doesn't conform to the rules we don't do anything.
	The terms to be done are put in special notation after the function.
	Notation: numsymbol, power, |coef|, sign
	If numsymbol is negative there is no symbol.
	We do it this way because otherwise stepping backwards (as in range=(4,1))
	would be very difficult.
*/
	tt = tstop; i = 0;
	while ( n <= num2 ) {
		if ( f >= tstop ) return(0);
		if ( *f == -SNUMBER ) { *tt++ = -1; *tt++ = 0;
			if ( f[1] < 0 ) { *tt++ = -f[1]; *tt++ = -1; }
			else { *tt++ = f[1]; *tt++ = 1; }
			f += 2;
		}
		else if ( *f == -SYMBOL ) { *tt++ = f[1]; *tt++ = 1; *tt++ = 1; *tt++ = 1; f += 2; }
		else if ( *f < 0 ) return(0);
		else {
			if ( *f != ( f[ARGHEAD]+ARGHEAD ) ) return(0); /* Not a single term */
			t = f + *f - 1;
			i1 = ABS(*t);
			if ( ( i1 > 3 ) || ( t[-1] != 1 ) ) return(0); /* Not an integer or too big */
			if ( (UWORD)(t[-2]) > MAXPOSITIVE2 ) return(0); /* number too big */
			if ( f[ARGHEAD] == i1+1 ) { /* numerical which is fine */
				*tt++ = -1; *tt++ = 0; *tt++ = t[-2];
				if ( *t < 0 ) { *tt++ = -1; }
				else { *tt++ = 1; }
			}
			else if ( ( f[ARGHEAD+1] != SYMBOL )
				 || ( f[ARGHEAD+2] != 4 )
				 || ( ( f+ARGHEAD+1+f[ARGHEAD+2] ) < ( t-i1 ) ) ) return(0);
					/* not a single symbol with a coefficient */
			else {
				*tt++ = f[ARGHEAD+3];
				*tt++ = f[ARGHEAD+4];
				*tt++ = t[-2];
				if ( *t < 0 ) { *tt++ = -1; }
				else { *tt++ = 1; }
			}
			f += *f;
		}
		i++; n++;
	}
	fff = f;
/*
	At this point we can do the implosion.
	Requirement: no coefficient shall take more than one word.
	(a stricter requirement may be needed to keep the explosion contained)
*/
	if ( arg1 > arg2 ) {
/*
		Work backward.
*/
		t = tt - 4; numzero = 0;
		while ( t >= tstop ) {
			if ( t[2] == 0 ) numzero++;
			else {
				if ( numzero > 0 ) {
					t[2] += numzero;
					t4 = t+4;
					ttt = t4 + 4*numzero;
					while ( ttt < tt ) *t4++ = *ttt++;
					tt -= 4*numzero;
					numzero = 0;
				}
			}
			t -= 4;
		}
	}
	else {
		t = tstop;
		numzero = 0; ttt = t;
		while ( t < tt ) {
			if ( t[2] == 0 ) numzero++;
			else {
				if ( numzero > 0 ) {
					t[2] += numzero;
					t4 = t;
					while ( t4 < tt ) *ttt++ = *t4++;
					tt -= 4*numzero;
					t -= 4*numzero;
					ttt = t + 4;
					numzero = 0;
				}
				else {
					ttt = t + 4;
				}
			}
			t += 4;
		}
/*
		We may have numzero > 0 at the end. We leave them.
		Output space is currently from tstop to tt
*/
	}
/*
	Now we compute the real output space needed
*/
	t = tstop; outspace = 0;
	while ( t < tt ) {
		if ( t[0] == -1 ) {
			if ( t[2] > MAXPOSITIVE2 ) { return(0); /* Number too big */ }
			outspace += 2;
		}
		else if ( t[1] == 1 && t[2] == 1 && t[3] == 1 ) { outspace += 2; }
		else { outspace += 8 + ARGHEAD; }
		t += 4;
	}
	if ( outspace < (fff-ff) ) {
		t = tstop;
		while ( t < tt ) {
			if ( t[0] == -1 ) { *ff++ = -SNUMBER; *ff++ = t[2]*t[3]; }
			else if ( t[1] == 1 && t[2] == 1 && t[3] == 1 ) {
				*ff++ = -SYMBOL; *ff++ = t[0];
			}
			else {
				*ff++ = 8+ARGHEAD; *ff++ = 0; FILLARG(ff);
				*ff++ = 8; *ff++ = SYMBOL; *ff++ = 4; *ff++ = t[0]; *ff++ = t[1];
				*ff++ = t[2]; *ff++ = 1; *ff++ = t[3] > 0 ? 3: -3;
			}
			t += 4;
		}
		while ( fff < tstop ) *ff++ = *fff++;
		fun[1] = ff - fun;
	}
	else if ( outspace > (fff-ff) ) {
/*
		Move the answer up by the required amount.
		Move the tail to its new location
		Move in things as for outspace == (fff-ff)
*/
		moveup = outspace-(fff-ff);
		ttt = tt + moveup;
		t = tt;
		while ( t > fff ) *--ttt = *--t;
		tt += moveup; tstop += moveup;
		fff += moveup;
		fun[1] += moveup;
		goto moveinto;
	}
	else {
moveinto:
		t = tstop;
		while ( t < tt ) {
			if ( t[0] == -1 ) { *ff++ = -SNUMBER; *ff++ = t[2]*t[3]; }
			else if ( t[1] == 1 && t[2] == 1 && t[3] == 1 ) {
				*ff++ = -SYMBOL; *ff++ = t[0];
			}
			else {
				*ff++ = 8+ARGHEAD; *ff++ = 0; FILLARG(ff);
				*ff++ = 8; *ff++ = SYMBOL; *ff++ = 4; *ff++ = t[0]; *ff++ = t[1];
				*ff++ = t[2]; *ff++ = 1; *ff++ = t[3] > 0 ? 3: -3;
			}
			t += 4;
		}
	}
	return(0);
}

/*
 		#] RunImplode : 
 		#[ RunExplode :
*/

WORD RunExplode(PHEAD WORD *fun, WORD *args)
{
	WORD arg1, arg2, num1, num2, *tt, *tstop, totarg, *tonew, *newfun;
	WORD *ff, *f;
	int reverse = 0, iarg, i, numzero;
	if ( functions[fun[0]-FUNCTION].spec != 0 ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunExplode");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
/*
	Get the proper range in forward direction and the number of arguments
*/
	if ( arg1 > arg2 ) { num1 = arg2; num2 = arg1; reverse = 1; }
	else { num1 = arg1; num2 = arg2; }
	if ( num1 > totarg || num2 > totarg ) return(0);
	if ( tstop + AM.MaxTer > AT.WorkTop ) goto OverWork;
/*
	We will make the new function after the old one in the workspace
	Find the first argument
*/
	tonew = newfun = tstop;
	ff = fun + FUNHEAD; iarg = 0;
	while ( ff < tstop ) {
		iarg++;
		if ( iarg == num1 ) {
			i = ff - fun; f = fun;
			NCOPY(tonew,f,i)
			break;
		}
		NEXTARG(ff)
	}
/*
	We have reached the first argument to be done
*/
	while ( iarg <= num2 ) {
		if ( *ff == -SYMBOL || ( *ff == -SNUMBER && ff[1] == 0 ) )
			{ *tonew++ = *ff++; *tonew++ = *ff++; }
		else if ( *ff == -SNUMBER ) {
			numzero = ABS(ff[1])-1;
			if ( reverse ) {
				*tonew++ = -SNUMBER; *tonew++ = ff[1] < 0 ? -1: 1;
				while ( numzero > 0 ) {
					*tonew++ = -SNUMBER; *tonew++ = 0; numzero--;
				}
			}
			else {
				while ( numzero > 0 ) {
					*tonew++ = -SNUMBER; *tonew++ = 0; numzero--;
				}
				*tonew++ = -SNUMBER; *tonew++ = ff[1] < 0 ? -1: 1;
			}
			ff += 2;
		}
		else if ( *ff < 0 ) { return(0); }
		else {
			if ( *ff != ARGHEAD+8 || ff[ARGHEAD] != 8
				 || ff[ARGHEAD+1] != SYMBOL || ABS(ff[ARGHEAD+7]) != 3
				 || ff[ARGHEAD+6] != 1 ) return(0);
			numzero = ff[ARGHEAD+5];
			if ( numzero >= MAXPOSITIVE2 ) return(0);
			numzero--;
			if ( reverse ) {
				if ( ff[ARGHEAD+7] > 0 ) { *tonew++ = -SNUMBER; *tonew++ = 1; }
				else {
					*tonew++ = ARGHEAD+8; *tonew++ = 0; FILLARG(tonew)
					*tonew++ = 8; *tonew++ = SYMBOL; *tonew++ = ff[ARGHEAD+3];
					*tonew++ = ff[ARGHEAD+4]; *tonew++ = 1; *tonew++ = 1;
					*tonew++ = -3;
				}
				while ( numzero > 0 ) {
					*tonew++ = -SNUMBER; *tonew++ = 0; numzero--;
				}
			}
			else {
				while ( numzero > 0 ) {
					*tonew++ = -SNUMBER; *tonew++ = 0; numzero--;
				}
				*tonew++ = ARGHEAD+8; *tonew++ = 0; FILLARG(tonew)
				*tonew++ = 8; *tonew++ = SYMBOL; *tonew++ = 4;
				*tonew++ = ff[ARGHEAD+3]; *tonew++ = ff[ARGHEAD+4];
				*tonew++ = 1; *tonew++ = 1;
				if ( ff[ARGHEAD+7] > 0 ) *tonew++ = 3;
				else                     *tonew++ = -3;
			}
			ff += *ff;
		}
		if ( tonew > AT.WorkTop ) goto OverWork;
		iarg++;
	}
/*
	Copy the tail, settle the size and copy the whole thing back.
*/
	while ( ff < tstop ) *tonew++ = *ff++;
	i = newfun[1] = tonew-newfun;
	NCOPY(fun,newfun,i)
	return(0);
OverWork:;
	MLOCK(ErrorMessageLock);
	MesWork();
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunExplode :
 		#[ RunPermute :
*/

WORD RunPermute(PHEAD WORD *fun, WORD *args, WORD *info)
{
	WORD *tt, totarg, *tstop, arg1, arg2, n, num, i, *f, *f1, *f2, *infostop;
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunPermute");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	  while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	  arg1 = 1; arg2 = totarg;
/*
	  We need to:
		1: get pointers to the arguments
		2: permute the pointers
		3: copy the arguments to safe territory in the new order
		4: copy this new order back in situ.
*/
	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; NEXTARG(f) }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; NEXTARG(f) }
/*
	  Now the permutations
*/
	  info++;
	  while ( *info ) {
		infostop = info + *info;
		info++;
		if ( *info > totarg ) return(0);
		tt = AT.pWorkSpace[AT.pWorkPointer+*info];
		info++;
		while ( info < infostop ) {
			if ( *info > totarg ) return(0);
			AT.pWorkSpace[AT.pWorkPointer+info[-1]] = AT.pWorkSpace[AT.pWorkPointer+*info];
			info++;
		}
		AT.pWorkSpace[AT.pWorkPointer+info[-1]] = tt;
	  }
/*
	  And the final cleanup
*/
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; COPY1ARG(f2,f) }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	else {  /* tensors */
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = tstop-tt;
	  arg1 = 1; arg2 = totarg;
	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; f++; }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; f++; }
/*
	  Now the permutations
*/
	  info++;
	  while ( *info ) {
		infostop = info + *info;
		info++;
		if ( *info > totarg ) return(0);
		tt = AT.pWorkSpace[AT.pWorkPointer+*info];
		info++;
		while ( info < infostop ) {
			if ( *info > totarg ) return(0);
			AT.pWorkSpace[AT.pWorkPointer+info[-1]] = AT.pWorkSpace[AT.pWorkPointer+*info];
			info++;
		}
		AT.pWorkSpace[AT.pWorkPointer+info[-1]] = tt;
	  }
/*
	  And the final cleanup
*/
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; *f2++= *f++; }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	return(0);
OverWork:;
	MLOCK(ErrorMessageLock);
	MesWork();
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunPermute : 
 		#[ RunReverse :
*/

WORD RunReverse(PHEAD WORD *fun, WORD *args)
{
	WORD *tt, totarg, *tstop, arg1, arg2, n, num, i, *f, *f1, *f2, i1, i2;
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunReverse");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	  while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	  arg1 = args[1];
	  if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	  arg2 = args[2];
	  if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
/*
	  We need to:
		1: get pointers to the arguments
		2: reverse the order of the pointers
		3: copy the arguments to safe territory in the new order
		4: copy this new order back in situ.
*/
	  if ( arg2 < arg1 ) { n = arg1; arg1 = arg2; arg2 = n; }
	  if ( arg2 > totarg ) return(0);

	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; NEXTARG(f) }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; NEXTARG(f) }
	  i1 = i-1; i2 = 0;
	  while ( i1 > i2 ) {
		tt = AT.pWorkSpace[AT.pWorkPointer+i1];
		AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
		AT.pWorkSpace[AT.pWorkPointer+i2] = tt;
		i1--; i2++;
	  }
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; COPY1ARG(f2,f) }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	else {	/* Tensors */
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = tstop - tt;
	  arg1 = args[1];
	  if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	  arg2 = args[2];
	  if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
/*
	  We need to:
		1: get pointers to the arguments
		2: reverse the order of the pointers
		3: copy the arguments to safe territory in the new order
		4: copy this new order back in situ.
*/
	  if ( arg2 < arg1 ) { n = arg1; arg1 = arg2; arg2 = n; }
	  if ( arg2 > totarg ) return(0);

	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; f++; }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; f++; }
	  i1 = i-1; i2 = 0;
	  while ( i1 > i2 ) {
		tt = AT.pWorkSpace[AT.pWorkPointer+i1];
		AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
		AT.pWorkSpace[AT.pWorkPointer+i2] = tt;
		i1--; i2++;
	  }
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; *f2++ = *f++; }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	return(0);
OverWork:;
	MLOCK(ErrorMessageLock);
	MesWork();
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunReverse : 
 		#[ RunCycle :
*/

WORD RunCycle(PHEAD WORD *fun, WORD *args, WORD *info)
{
	WORD *tt, totarg, *tstop, arg1, arg2, n, num, i, j, *f, *f1, *f2, x;
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunCycle");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	if ( functions[fun[0]-FUNCTION].spec != TENSORFUNCTION ) {
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	  while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	  arg1 = args[1]; arg2 = args[2];
	  if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	  if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	  if ( arg1 > arg2 ) { n = arg1; arg1 = arg2; arg2 = n; }
	  if ( arg2 > totarg ) return(0);
/*
	  We need to:
		1: get pointers to the arguments
		2: cycle the pointers
		3: copy the arguments to safe territory in the new order
		4: copy this new order back in situ.
*/
	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; NEXTARG(f) }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; NEXTARG(f) }
/*
	  Now the cycle(s). First minimize the number of cycles.
*/
	  info++;
	  x = *info;
	  if ( x >= i ) {
		x %= i;
		if ( x > i/2 ) x -= i;
	  }
	  else if ( x <= -i ) {
		x = -((-x) % i);
		if ( x <= -i/2 ) x += i;
	  }
	  while ( x ) {
		if ( x > 0 ) {
			tt = AT.pWorkSpace[AT.pWorkPointer+i-1];
			for ( j = i-1; j > 0; j-- )
				AT.pWorkSpace[AT.pWorkPointer+j] = AT.pWorkSpace[AT.pWorkPointer+j-1];
			AT.pWorkSpace[AT.pWorkPointer] = tt;
			x--;
		}
		else {
			tt = AT.pWorkSpace[AT.pWorkPointer];
			for ( j = 1; j < i; j++ )
				AT.pWorkSpace[AT.pWorkPointer+j-1] = AT.pWorkSpace[AT.pWorkPointer+j];
			AT.pWorkSpace[AT.pWorkPointer+j-1] = tt;
			x++;
		}
	  }
/*
	  And the final cleanup
*/
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; COPY1ARG(f2,f) }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	else {	/* Tensors */
	  tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = tstop - tt;
	  arg1 = args[1]; arg2 = args[2];
	  if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	  if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	  if ( arg1 > arg2 ) { n = arg1; arg1 = arg2; arg2 = n; }
	  if ( arg2 > totarg ) return(0);
/*
	  We need to:
		1: get pointers to the arguments
		2: cycle the pointers
		3: copy the arguments to safe territory in the new order
		4: copy this new order back in situ.
*/
	  num = arg2-arg1+1;
	  WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	  f = fun+FUNHEAD; n = 1; i = 0;
	  while ( n < arg1 ) { n++; f++; }
	  f1 = f;
	  while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; f++; }
/*
	  Now the cycle(s). First minimize the number of cycles.
*/
	  info++;
	  x = *info;
	  if ( x >= i ) {
		x %= i;
		if ( x > i/2 ) x -= i;
	  }
	  else if ( x <= -i ) {
		x = -((-x) % i);
		if ( x <= -i/2 ) x += i;
	  }
	  while ( x ) {
		if ( x > 0 ) {
			tt = AT.pWorkSpace[AT.pWorkPointer+i-1];
			for ( j = i-1; j > 0; j-- )
				AT.pWorkSpace[AT.pWorkPointer+j] = AT.pWorkSpace[AT.pWorkPointer+j-1];
			AT.pWorkSpace[AT.pWorkPointer] = tt;
			x--;
		}
		else {
			tt = AT.pWorkSpace[AT.pWorkPointer];
			for ( j = 1; j < i; j++ )
				AT.pWorkSpace[AT.pWorkPointer+j-1] = AT.pWorkSpace[AT.pWorkPointer+j];
			AT.pWorkSpace[AT.pWorkPointer+j-1] = tt;
			x++;
		}
	  }
/*
	  And the final cleanup
*/
	  if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	  f2 = tstop;
	  for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; *f2++ = *f++; }
	  i = f2 - tstop;
	  NCOPY(f1,tstop,i)
	}
	return(0);
OverWork:;
	MLOCK(ErrorMessageLock);
	MesWork();
	MUNLOCK(ErrorMessageLock);
	return(-1);
}

/*
 		#] RunCycle : 
 		#[ RunIsLyndon :

		Determines whether the range constitutes a Lyndon word.
		The two cases of ordering are distinguised by the order of
		the numbers of the arguments in the range.
*/

WORD RunIsLyndon(PHEAD WORD *fun, WORD *args, int par)
{
	WORD *tt, totarg, *tstop, arg1, arg2, arg, num, *f, n, i;
/*	WORD *f1; */
	WORD sign, i1, i2, retval;
	if ( fun[0] <= GAMMASEVEN && fun[0] >= GAMMA ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunIsLyndon");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	if ( arg1 > totarg || arg2 > totarg ) return(-1);
/*
	Now make a list of the relevant arguments.
*/
	if ( arg1 == arg2 ) return(1);
	if ( arg2 < arg1 ) {	/* greater, rather than smaller */
		arg = arg1; arg1 = arg2; arg2 = arg; sign = 1;
	}
	else sign = 0;

	num = arg2-arg1+1;
	WantAddPointers(num);	/* Guarantees the presence of enough pointers */
	f = fun+FUNHEAD; n = 1; i = 0;
	while ( n < arg1 ) { n++; NEXTARG(f) }
/*	f1 = f; */
	while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; NEXTARG(f) }
/*
	If sign == 1 we should alter the order of the pointers first
*/
	if ( sign ) {
		i1 = i-1; i2 = 0;
		while ( i1 > i2 ) {
			tt = AT.pWorkSpace[AT.pWorkPointer+i1];
			AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
			AT.pWorkSpace[AT.pWorkPointer+i2] = tt;
			i1--; i2++;
		}
	}
/*
	The argument range is from f1 to f and the num pointers to the arguments
	are in AT.pWorkSpace[AT.pWorkPointer] to AT.pWorkSpace[AT.pWorkPointer+num-1]
*/
	for ( i1 = 1; i1 < num; i1++ ) {
		retval = par * CompArg(AT.pWorkSpace[AT.pWorkPointer+i1],
							AT.pWorkSpace[AT.pWorkPointer]);
		if ( retval > 0 ) continue;
		if ( retval < 0 ) return(0);
        for ( i2 = 1; i2 < num; i2++ ) {
			retval = par * CompArg(AT.pWorkSpace[AT.pWorkPointer+(i1+i2)%num],
							AT.pWorkSpace[AT.pWorkPointer+i2]);
			if ( retval < 0 ) return(0);
			if ( retval > 0 ) goto nexti1;
		}
/*
		If we come here the sequence is not unique.
*/
		return(0);
nexti1:;
	}
	return(1);
}

/*
 		#] RunIsLyndon : 
 		#[ RunToLyndon :

		Determines whether the range constitutes a Lyndon word.
		If not, we rotate it to a Lyndon word. If this is not possible
		we return the noLyndon condition.
		The two cases of ordering are distinguised by the order of
		the numbers of the arguments in the range.
*/

WORD RunToLyndon(PHEAD WORD *fun, WORD *args, int par)
{
	WORD *tt, totarg, *tstop, arg1, arg2, arg, num, *f, *f1, *f2, n, i;
	WORD sign, i1, i2, retval, unique;
	if ( fun[0] <= GAMMASEVEN && fun[0] >= GAMMA ) return(0);
	if ( *args != ARGRANGE ) {
		MLOCK(ErrorMessageLock);
		MesPrint("Illegal range encountered in RunToLyndon");
		MUNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
	tt = fun+FUNHEAD; tstop = fun+fun[1]; totarg = 0;
	while ( tt < tstop ) { totarg++; NEXTARG(tt); }
	arg1 = args[1];
	if ( arg1 >= MAXPOSITIVE2 ) { arg1 = totarg-(arg1-MAXPOSITIVE2); }
	arg2 = args[2];
	if ( arg2 >= MAXPOSITIVE2 ) { arg2 = totarg-(arg2-MAXPOSITIVE2); }
	if ( arg1 > totarg || arg2 > totarg ) return(-1);
/*
	Now make a list of the relevant arguments.
*/
	if ( arg1 == arg2 ) return(1);
	if ( arg2 < arg1 ) {	/* greater, rather than smaller */
		arg = arg1; arg1 = arg2; arg2 = arg; sign = 1;
	}
	else sign = 0;

	num = arg2-arg1+1;
	WantAddPointers((2*num));	/* Guarantees the presence of enough pointers */
	f = fun+FUNHEAD; n = 1; i = 0;
	while ( n < arg1 ) { n++; NEXTARG(f) }
	f1 = f;
	while ( n <= arg2 ) { AT.pWorkSpace[AT.pWorkPointer+i++] = f; n++; NEXTARG(f) }
/*
	If sign == 1 we should alter the order of the pointers first
*/
	if ( sign ) {
		i1 = i-1; i2 = 0;
		while ( i1 > i2 ) {
			tt = AT.pWorkSpace[AT.pWorkPointer+i1];
			AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
			AT.pWorkSpace[AT.pWorkPointer+i2] = tt;
			i1--; i2++;
		}
	}
/*
	The argument range is from f1 to f and the num pointers to the arguments
	are in AT.pWorkSpace[AT.pWorkPointer] to AT.pWorkSpace[AT.pWorkPointer+num-1]
*/
	unique = 1;
	for ( i1 = 1; i1 < num; i1++ ) {
		retval = par * CompArg(AT.pWorkSpace[AT.pWorkPointer+i1],
							AT.pWorkSpace[AT.pWorkPointer]);
		if ( retval > 0 ) continue;
		if ( retval < 0 ) {
Rotate:;
/*
			Rotate so that i1 becomes the zero element. Then start again.
*/
			for ( i2 = 0; i2 < num; i2++ ) {
				AT.pWorkSpace[AT.pWorkPointer+num+i2] =
					AT.pWorkSpace[AT.pWorkPointer+(i1+i2)%num];
			}
			for ( i2 = 0; i2 < num; i2++ ) {
				AT.pWorkSpace[AT.pWorkPointer+i2] =
					AT.pWorkSpace[AT.pWorkPointer+i2+num];
			}
			i1 = 0;
			goto nexti1;
		}
        for ( i2 = 1; i2 < num; i2++ ) {
			retval = par * CompArg(AT.pWorkSpace[AT.pWorkPointer+(i1+i2)%num],
							AT.pWorkSpace[AT.pWorkPointer+i2]);
			if ( retval < 0 ) goto Rotate;
			if ( retval > 0 ) goto nexti1;
		}
/*
		If we come here the sequence is not unique.
*/
		unique = 0;
nexti1:;
	}
	if ( sign ) {
		i1 = i-1; i2 = 0;
		while ( i1 > i2 ) {
			tt = AT.pWorkSpace[AT.pWorkPointer+i1];
			AT.pWorkSpace[AT.pWorkPointer+i1] = AT.pWorkSpace[AT.pWorkPointer+i2];
			AT.pWorkSpace[AT.pWorkPointer+i2] = tt;
			i1--; i2++;
		}
	}
/*
	Now rewrite the arguments into the proper order
*/
	if ( tstop+(f-f1) > AT.WorkTop ) goto OverWork;
	f2 = tstop;
	for ( i = 0; i < num; i++ ) { f = AT.pWorkSpace[AT.pWorkPointer+i]; COPY1ARG(f2,f) }
	i = f2 - tstop;
	NCOPY(f1,tstop,i)
/*
	The return value indicates whether we have a Lyndon word
*/
	return(unique);
OverWork:;
	MLOCK(ErrorMessageLock);
	MesWork();
	MUNLOCK(ErrorMessageLock);
	return(-2);
}

/*
 		#] RunToLyndon : 
 		#[ TestArgNum :

		Looks whether argument n is contained in any of the ranges
		specified in args. Args contains objects of the types
			ALLARGS
			NUMARG,num
			ARGRANGE,num1,num2
		The object MAKEARGS,num1,num2 is skipped
		Any other object terminates the range specifications.
*/

int TestArgNum(int n, int totarg, WORD *args)
{
	WORD x1, x2;
	for(;;) {
		switch ( *args ) {
			case ALLARGS:
				return(1);
			case NUMARG:
				if ( n == args[1] ) return(1);
				if ( args[1] >= MAXPOSITIVE2 ) {
					x1 = args[1]-MAXPOSITIVE2;
					if ( totarg-x1 == n ) return(1);
				}
				args += 2;
				break;
			case ARGRANGE:
				if ( args[1] >= MAXPOSITIVE2 ) {
					x1 = totarg-(args[1]-MAXPOSITIVE2);
				}
				else x1 = args[1];
				if ( args[2] >= MAXPOSITIVE2 ) {
					x2 = totarg-(args[2]-MAXPOSITIVE2);
				}
				else x2 = args[2];
				if ( x1 >= x2 ) {
					if ( n >= x2 && n <= x1 ) return(1);
				}
				else {
					if ( n >= x1 && n <= x2 ) return(1);
				}
				args += 3;
				break;
			case MAKEARGS:
				args += 3;
				break;
			default:
				return(0);
		}
	}
}

/*
 		#] TestArgNum : 
 		#[ PutArgInScratch :
*/

WORD PutArgInScratch(WORD *arg,UWORD *scrat)
{
	WORD size, *t, i;
	if ( *arg == -SNUMBER ) {
		scrat[0] = ABS(arg[1]);
		if ( arg[1] < 0 ) size = -1;
		else              size =  1;
	}
	else {
		t = arg+*arg-1;
		if ( *t < 0 ) { i = ((-*t)-1)/2; size = -i; }
		else          { i = (  *t -1)/2; size =  i; }
		t = arg+ARGHEAD+1;
		NCOPY(scrat,t,i);
	}
	return(size);
}

/*
 		#] PutArgInScratch : 
 		#[ ReadRange :

		Comes in at the bracket and leaves at the = sign
		Ranges can be:
			#1,#2  with # numbers. If the second is smaller than the
					first we work it backwards.
			first,#2 or #2,first
			#1,last  or last,#1
			first,last or last,first
		First is represented by 1. Last is represented by MAXPOSITIVE2.

		par = 0: we need the = after.
		par = 1: we need a , or '\0' after.
		par = 2: we need a :
*/

UBYTE *ReadRange(UBYTE *s, WORD *out, int par)
{
	UBYTE *in = s, *ss, c;
	LONG x1, x2;

	SKIPBRA3(in)
	if ( par == 0 && in[1] != '=' ) {
		MesPrint("&A range in this type of transform statement should be followed by a = sign");
		return(0);
	}
	else if ( par == 1 && in[1] != ',' && in[1] != '\0' ) {
		MesPrint("&A range in this type of transform statement should be followed by a comma or end-of-statement");
		return(0);
	}
	else if ( par == 2 && in[1] != ':' ) {
		MesPrint("&A range in this type of transform statement should be followed by a :");
		return(0);
	}
	s++;
	if ( FG.cTable[*s] == 0 ) {
		ss = s; while ( FG.cTable[*s] == 0 ) s++;
		c = *s; *s = 0;
		if ( StrICmp(ss,(UBYTE *)"first") == 0 ) {
			*s = c;
			x1 = 1;
		}
		else if ( StrICmp(ss,(UBYTE *)"last") == 0 ) {
			*s = c;
			if ( c == '-' ) {
				s++; x1 = 0;
				while ( *s >= '0' && *s <= '9' ) {
					x1 = 10*x1 + *s++ - '0';
					if ( x1 >= MAXPOSITIVE2 ) {
						MesPrint("&Fixed range indicator bigger than %l",(LONG)MAXPOSITIVE2);
						return(0);
					}
				}
				x1 += MAXPOSITIVE2;
			}
			else x1 = MAXPOSITIVE2;
		}
		else {
			MesPrint("&Illegal keyword inside range specification");
			return(0);
		}
	}
	else if ( FG.cTable[*s] == 1 ) {
		x1 = 0;
		while ( *s >= '0' && *s <= '9' ) {
			x1 = x1*10 + *s++ - '0';
			if ( x1 >= MAXPOSITIVE2 ) {
				MesPrint("&Fixed range indicator bigger than %l",(LONG)MAXPOSITIVE2);
				return(0);
			}
		}
	}
	else {
		MesPrint("&Illegal character in range specification");
		return(0);
	}
	if ( *s != ',' ) {
		MesPrint("&A range is two indicators, separated by a comma or blank");
		return(0);
	}
	s++;
	if ( FG.cTable[*s] == 0 ) {
		ss = s; while ( FG.cTable[*s] == 0 ) s++;
		c = *s; *s = 0;
		if ( StrICmp(ss,(UBYTE *)"first") == 0 ) {
			*s = c;
			x2 = 1;
		}
		else if ( StrICmp(ss,(UBYTE *)"last") == 0 ) {
			*s = c;
			if ( c == '-' ) {
				s++; x2 = 0;
				while ( *s >= '0' && *s <= '9' ) {
					x2 = 10*x2 + *s++ - '0';
					if ( x2 >= MAXPOSITIVE2 ) {
						MesPrint("&Fixed range indicator bigger than %l",(LONG)MAXPOSITIVE2);
						return(0);
					}
				}
				x2 += MAXPOSITIVE2;
			}
			else x2 = MAXPOSITIVE2;
		}
		else {
			MesPrint("&Illegal keyword inside range specification");
			return(0);
		}
	}
	else if ( FG.cTable[*s] == 1 ) {
		x2 = 0;
		while ( *s >= '0' && *s <= '9' ) {
			x2 = x2*10 + *s++ - '0';
			if ( x2 >= MAXPOSITIVE2 ) {
				MesPrint("&Fixed range indicator bigger than %l",(LONG)MAXPOSITIVE2);
				return(0);
			}
		}
	}
	else {
		MesPrint("&Illegal character in range specification");
		return(0);
	}
	if ( s < in ) {
		MesPrint("&A range is two indicators, separated by a comma or blank between parentheses");
		return(0);
	}
	out[0] = x1; out[1] = x2;
	return(in+1);
}

/*
 		#] ReadRange : 
 	#] Transform :
*/

