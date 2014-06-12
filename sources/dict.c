/** @file dict.c
 * 
 *	Contains the code pertaining to dictionaries
 *	Commands are:
 *		#opendictionary name
 *		#closedictionary
 *		#selectdictionary name <options>
 *	There can be several dictionaries, but only one can be active.
 *	Defining elements is done with
 *		#add object: "replacement"
 *	Replacements are strings when a dictionary is for output translation.
 *	Objects can be
 *		1: a number (rational)
 *		2: a variable
 *		3: * ^
 *		4: a function with arguments
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
  	#[ Includes : ratio.c

	Data setup:
		AO.Dictionaries          Array of pointers to DICTIONARY
		AO.NumDictionaries
		AO.SizeDictionaries
		AO.CurrentDictionary
		AO.CurDictNumbers
		AO.CurDictVariables
		AO.CurDictSpecials
		AP.OpenDictionary
*/

#include "form3.h"

/*
  	#] Includes : 
  	#[ TransformRational:

	Tries to transform the rational number a according to the rules of
	the current dictionary. Whatever cannot be translated goes to the
	regular output.
	Options for AO.CurDictNumbers are:
		DICT_ALLNUMBERS, DICT_RATIONALONLY, DICT_INTEGERONLY, DICT_NONUMBERS
*/

VOID TransformRational(UWORD *a, WORD na)
{
	DICTIONARY *dict;
	WORD i, j, nb, i1, i2; UWORD *b;
	if ( AO.CurrentDictionary <= 0 ) goto NoAction;
	dict = AO.Dictionaries[AO.CurrentDictionary-1];
	if ( na < 0 ) na = -na;
	switch ( AO.CurDictNumbers ) {
		case DICT_NONUMBERS:
			goto NoAction;
		case DICT_INTEGERONLY:
			if ( a[na] != 1 ) goto NoAction;
			if ( na > 1 ) {
				for ( i = 1; i < na; i++ ) {
					if ( a[na+i] != 0 ) goto NoAction;
				}
			}
Numeratoronly:;
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_INTEGERNUMBER ) {
					if ( dict->elements[i]->size == na ) {
						for ( j = 0; j < na; j++ ) {
							if ( (UWORD)(dict->elements[i]->lhs[j]) != a[j] ) break;
						}
						if ( j == na ) {	/* Got it */
							TokenToLine((UBYTE *)(dict->elements[i]->rhs));
							return;
						}
					}
				}
			}
			goto NotFound;
		case DICT_RATIONALONLY:
			nb = 2*na;
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_RATIONALNUMBER ) {
					if ( dict->elements[i]->size == nb+2 ) {
						for ( j = 0; j < nb; j++ ) {
							if ( (UWORD)(dict->elements[i]->lhs[j+1]) != a[j] ) break;
						}
						if ( j == nb ) {	/* Got it */
							TokenToLine((UBYTE *)(dict->elements[i]->rhs));
							return;
						}
					}
				}
			}
			goto NotFound;
		case DICT_ALLNUMBERS:
/*
			First fish for rationals
*/
			nb = 2*na;
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_RATIONALNUMBER ) {
					if ( dict->elements[i]->size == nb+2 ) {
						for ( j = 0; j < nb; j++ ) {
							if ( (UWORD)(dict->elements[i]->lhs[j+1]) != a[j] ) break;
						}
						if ( j == nb ) {	/* Got it */
							TokenToLine((UBYTE *)(dict->elements[i]->rhs));
							return;
						}
					}
				}
			}
/*
			Now look for element[j1]/element[j2]
*/
			nb = na; b = a+na;
			while ( b[nb-1] == 0 ) nb--;
			if ( nb == 1 && b[0] == 1 ) goto Numeratoronly;
			while ( a[na-1] == 0 ) na--;
			for ( i1 = dict->numelements-1; i1 >= 0; i1-- ) {
				if ( dict->elements[i1]->type == DICT_INTEGERNUMBER ) {
					if ( dict->elements[i1]->size == na ) {
						for ( j = 0; j < na; j++ ) {
							if ( (UWORD)(dict->elements[i1]->lhs[j]) != a[j] ) break;
						}
						if ( j == na ) break;
					}
				}
			}
			for ( i2 = dict->numelements-1; i2 >= 0; i2-- ) {
				if ( dict->elements[i2]->type == DICT_INTEGERNUMBER ) {
					if ( dict->elements[i2]->size == nb ) {
						for ( j = 0; j < nb; j++ ) {
							if ( (UWORD)(dict->elements[i2]->lhs[j]) != b[j] ) break;
						}
						if ( j == nb ) break;
					}
				}
			}
			if ( i1 < 0 ) {
				if ( i2 < 0 ) goto NotFound;
				else {	/* number/replacement[i2] */
					LongToLine(a,na);
					if ( na > 1 ) {
						if ( AC.OutputMode == FORTRANMODE || AC.OutputMode == PFORTRANMODE
							 || AC.OutputMode == CMODE ) {
							if ( AO.DoubleFlag == 2 ) { AddToLine((UBYTE *)".Q0/"); }
							else if ( AO.DoubleFlag == 1 ) { AddToLine((UBYTE *)".D0/"); }
							else { AddToLine((UBYTE *)"/"); }
						}
					}
					else AddToLine((UBYTE *)("/"));
					TokenToLine((UBYTE *)(dict->elements[i2]->rhs));
				}
			}
			else if ( i2 < 0 ) { /* replacement[i1]/number */
				TokenToLine((UBYTE *)(dict->elements[i1]->rhs));
				AddToLine((UBYTE *)("/"));
				LongToLine((UWORD *)(b),nb);
				if ( nb > 1 ) {
					if ( AC.OutputMode == FORTRANMODE || AC.OutputMode == PFORTRANMODE
						 || AC.OutputMode == CMODE ) {
						if ( AO.DoubleFlag == 2 ) { AddToLine((UBYTE *)".Q0"); }
						else if ( AO.DoubleFlag == 1 ) { AddToLine((UBYTE *)".D0"); }
					}
				}
			}
			else { /* replacement[i1]/replacement[i2] */
				TokenToLine((UBYTE *)(dict->elements[i1]->rhs));
				AddToLine((UBYTE *)("/"));
				TokenToLine((UBYTE *)(dict->elements[i2]->rhs));
			}
			break;
		default:
			MesPrint("Illegal code in TransformRational: %d",AO.CurDictNumbers);
			Terminate(-1);
	}
	return;
NotFound:
	if ( AO.CurDictNumberWarning ) {
		MesPrint(">>>>>>>>Could not translate coefficient with dictionary %s<<<<<<<<<<<<",dict->name);
	}
NoAction:
	RatToLine(a,na);
	return;
}

/*
  	#] TransformRational: 
  	#[ IsMultiplySign:
*/

int IsMultiplySign(VOID)
{
	DICTIONARY *dict;
	int i;
	if ( AO.CurrentDictionary <= 0 ) return(0);
	dict = AO.Dictionaries[AO.CurrentDictionary-1];
	if ( dict->characters == 0 ) return(0);
	for ( i = dict->numelements-1; i >= 0; i-- ) {
		if ( ( dict->elements[i]->type == DICT_SPECIALCHARACTER )
			&& ( dict->elements[i]->lhs[0] == (WORD)('*') ) ) return(i+1);
	}
	return(0);
}

/*
  	#] IsMultiplySign: 
  	#[ IsExponentSign:
*/

int IsExponentSign(VOID)
{
	DICTIONARY *dict;
	int i;
	if ( AO.CurrentDictionary <= 0 ) return(0);
	dict = AO.Dictionaries[AO.CurrentDictionary-1];
	if ( dict->characters == 0 ) return(0);
	for ( i = dict->numelements-1; i >= 0; i-- ) {
		if ( ( dict->elements[i]->type == DICT_SPECIALCHARACTER )
			&& ( dict->elements[i]->lhs[0] == (WORD)('^') ) ) return(i+1);
	}
	return(0);
}

/*
  	#] IsExponentSign: 
  	#[ FindSymbol :
*/

UBYTE *FindSymbol(WORD num)
{
	if ( AO.CurrentDictionary > 0 ) {
		DICTIONARY *dict = AO.Dictionaries[AO.CurrentDictionary-1];
		int i;
		if ( dict->variables > 0 && AO.CurDictVariables == DICT_DOVARIABLES ) {
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_SYMBOL &&
				     dict->elements[i]->lhs[0] == num )
						return((UBYTE *)(dict->elements[i]->rhs));
			}
		}
	}
	return(VARNAME(symbols,num));
}

/*
  	#] FindSymbol : 
  	#[ FindVector :
*/

UBYTE *FindVector(WORD num)
{
	num -= AM.OffsetVector;
	if ( AO.CurrentDictionary > 0 ) {
		DICTIONARY *dict = AO.Dictionaries[AO.CurrentDictionary-1];
		int i;
		if ( dict->variables > 0 && AO.CurDictVariables == DICT_DOVARIABLES ) {
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_VECTOR &&
				     dict->elements[i]->lhs[0] == num )
						return((UBYTE *)(dict->elements[i]->rhs));
			}
		}
	}
	return(VARNAME(vectors,num));
}

/*
  	#] FindVector : 
  	#[ FindIndex :
*/

UBYTE *FindIndex(WORD num)
{
	num -= AM.OffsetIndex;
	if ( AO.CurrentDictionary > 0 ) {
		DICTIONARY *dict = AO.Dictionaries[AO.CurrentDictionary-1];
		int i;
		if ( dict->variables > 0 && AO.CurDictVariables == DICT_DOVARIABLES ) {
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_INDEX &&
				     dict->elements[i]->lhs[0] == num )
						return((UBYTE *)(dict->elements[i]->rhs));
			}
		}
	}
	return(VARNAME(indices,num));
}

/*
  	#] FindIndex : 
  	#[ FindFunction :
*/

UBYTE *FindFunction(WORD num)
{
	num -= FUNCTION;
	if ( AO.CurrentDictionary > 0 ) {
		DICTIONARY *dict = AO.Dictionaries[AO.CurrentDictionary-1];
		int i;
		if ( dict->variables > 0 && AO.CurDictVariables == DICT_DOVARIABLES ) {
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_FUNCTION &&
				     dict->elements[i]->lhs[0] == num )
						return((UBYTE *)(dict->elements[i]->rhs));
			}
		}
	}
	return(VARNAME(functions,num));
}

/*
  	#] FindFunction : 
  	#[ FindFunWithArgs :
*/

UBYTE *FindFunWithArgs(WORD *t)
{
	if ( AO.CurrentDictionary > 0 ) {
		DICTIONARY *dict = AO.Dictionaries[AO.CurrentDictionary-1];
		int i, j;
		if ( dict->funwith > 0
					&& AO.CurDictFunWithArgs == DICT_DOFUNWITHARGS ) {
			for ( i = dict->numelements-1; i >= 0; i-- ) {
				if ( dict->elements[i]->type == DICT_FUNCTION_WITH_ARGUMENTS &&
				     (WORD)(dict->elements[i]->lhs[0]) == t[0] &&
				     (WORD)(dict->elements[i]->lhs[1]) == t[1] ) {
					for ( j = 2; j < t[1]; j++ ) {
						if ( (WORD)(dict->elements[i]->lhs[j]) != t[j] ) break;
					}
					if ( j >= t[1] ) return((UBYTE *)(dict->elements[i]->rhs));
				}
			}
		}
	}
	return(0);
}

/*
  	#] FindFunWithArgs : 
  	#[ FindDictionary :
*/

int FindDictionary(UBYTE *name)
{
	int i;
	for ( i = 0; i < AO.NumDictionaries; i++ ) {
		if ( StrCmp(AO.Dictionaries[i]->name,name) == 0 )
			return(i+1);
	}
	return(0);
}

/*
  	#] FindDictionary : 
  	#[ AddDictionary :
*/

int AddDictionary(UBYTE *name)
{
	DICTIONARY *dict;
/*
	First make space for the pointer in the list.
*/
	if ( AO.NumDictionaries >= AO.SizeDictionaries-1 ) {
		DICTIONARY **d;
		int i;
		if ( AO.SizeDictionaries <= 0 ) AO.SizeDictionaries = 10;
		else AO.SizeDictionaries = 2*AO.SizeDictionaries;
		d = (DICTIONARY **)Malloc1(AO.SizeDictionaries*sizeof(DICTIONARY *),"Dictionaries");
		for ( i = 0; i < AO.NumDictionaries; i++ ) d[i] = AO.Dictionaries[i];
		if ( AO.Dictionaries != 0 ) M_free(AO.Dictionaries,"Dictionaries");
		AO.Dictionaries = d;
	}
/*
	Now create an empty dictionary.
*/
	dict = (DICTIONARY *)Malloc1(sizeof(DICTIONARY),"Dictionary");
	AO.Dictionaries[AO.NumDictionaries++] = dict;
	dict->elements = 0;
	dict->name = strDup1(name,"DictionaryName");
	dict->sizeelements = 0;
	dict->numelements = 0;
	dict->numbers = 0;
	dict->variables = 0;
	dict->characters = 0;
	dict->funwith = 0;
	dict->gnumelements = 0;
	dict->reserved = 0;

	return(AO.NumDictionaries);
}

/*
  	#] AddDictionary : 
  	#[ AddToDictionary :

		To be called from #add left:right
*/

int AddToDictionary(DICTIONARY *dict,UBYTE *left,UBYTE *right)
{
	GETIDENTITY
	CBUF *C = cbuf+AC.cbufnum;
	WORD *w = AT.WorkPointer;
	WORD *OldWork = AT.WorkPointer;
	WORD *s, oldnumrhs = C->numrhs, oldnumlhs = C->numlhs;
	WORD *ow, *ww, *mm, oldEside, *where = 0, type, number;
	LONG oldcpointer;
	int error = 0, sizelhs, sizerhs, i, retcode;
	UBYTE *r;
	DICTIONARY_ELEMENT *new;
	WORD power = (WORD)('^'), times = (WORD)('*');
	if ( ( left[0] == '^' && left[1] == 0 )
	  || ( left[0] == '*' && left[1] == '*' && left[2] == 0 ) ) {
		type = DICT_SPECIALCHARACTER;
		number = 1;
		where = &power;
		goto TestDouble;
	}
	else if ( left[0] == '*' && left[1] == 0 ) {
		type = DICT_SPECIALCHARACTER;
		number = 1;
		where = &times;
		goto TestDouble;
	}
/*
	Translate the left part. Determine type.
	We follow the code in CoIdExpression and then veto what we do not like.
	Just make sure to pop what needs to be popped in the compiler buffer.
*/
	AC.ProtoType = w;
	*w++ = SUBEXPRESSION;
	*w++ = SUBEXPSIZE;
	*w++ = C->numrhs+1;
	*w++ = 1;
	*w++ = AC.cbufnum;
	FILLSUB(w)
	AC.WildC = w;
	AC.NwildC = 0;
	AT.WorkPointer = s = w + 4*AM.MaxWildcards + 8;
/*
	Now read the LHS
*/
	oldcpointer = AddLHS(AC.cbufnum) - C->Buffer;

	if ( ( retcode = CompileAlgebra(left,LHSIDE,AC.ProtoType) ) < 0 ) { error = 1; }
	else AC.ProtoType[2] = retcode;
	AT.WorkPointer = s;
	if ( AC.NwildC && SortWild(w,AC.NwildC) ) error = 1;

	OldWork[1] = AC.WildC-OldWork;
	w = AC.WildC;
	AT.WorkPointer = w;
	s = C->rhs[C->numrhs];
/*
	We have the expression in the compiler buffers.
	The main level is at lhs[numlhs]
	The partial lhs (including ProtoType) is in OldWork (in WorkSpace)
	We need to load the result at w after the prototype
	Because these sort routines don't use the WorkSpace
	there should not be a conflict
*/
	if ( !error && *s == 0 ) {
IllLeft:MesPrint("&Illegal LHS in dictionary");
		AC.lhdollarflag = 0;
		return(1);
	}
	if ( !error && *(s+*s) != 0 ) {
		MesPrint("&LHS in dictionary should be one term only");
		return(1);
	}
	if ( error == 0 ) {
		if ( NewSort(BHEAD0) || NewSort(BHEAD0) ) {
			if ( !error ) error = 1;
			return(error);
		}
		AN.RepPoint = AT.RepCount + 1;
        ow = (WORD *)(((UBYTE *)(AT.WorkPointer)) + AM.MaxTer);
		mm = s; ww = ow; i = *mm;
		while ( --i >= 0 ) *ww++ = *mm++; AT.WorkPointer = ww;
		AC.lhdollarflag = 0; oldEside = AR.Eside; AR.Eside = LHSIDE;
		AR.Cnumlhs = C->numlhs;
		if ( Generator(BHEAD ow,C->numlhs) ) {
			AR.Eside = oldEside;
			LowerSortLevel(); LowerSortLevel(); goto IllLeft;
		}
		AR.Eside = oldEside;
		AT.WorkPointer = w;
		if ( EndSort(BHEAD w,0) < 0 ) { LowerSortLevel(); goto IllLeft; }
		if ( *w == 0 || *(w+*w) != 0 ) {
			MesPrint("&LHS must be one term");
			AC.lhdollarflag = 0;
			return(1);
		}
		LowerSortLevel();
	}
	AT.WorkPointer = w + *w;
	AC.DumNum = 0;
/*
	Everything is now after OldWork. We can pop the compilerbuffer.
	Next test for illegal things like a coefficient
	At this point we have:
	w = the term of the LHS
*/
	C->Pointer = C->Buffer + oldcpointer;
	C->numrhs = oldnumrhs;
	C->numlhs = oldnumlhs;
	AC.lhdollarflag = 0;
/*
	Test for undesirables.
		1: wildcards
		2: sign
		3: more than one term
		4: composite terms
*/
	if ( AC.ProtoType[1] != SUBEXPSIZE ) {
		MesPrint("& Currently no wildcards allowed in dictionaries.");
		return(1);
	}
	if ( w[w[0]-1] < 0 ) {
		MesPrint("& Currently no sign allowed in dictionaries.");
		return(1);
	}
	if ( w[w[0]] != 0 ) {
		MesPrint("& More than one term in dictionary element.");
		return(1);
	}
	if ( w[0] == w[w[0]-1]+1 ) {	/* Only coefficient */
		WORD *numer, *denom;
		WORD nsize, dsize;
		nsize = dsize = (w[w[0]-1]-1)/2;
		numer = w+1;
		denom = numer+nsize;
		while ( numer[nsize-1] == 0 ) nsize--;
		while ( denom[dsize-1] == 0 ) dsize--;
		if ( dsize == 1 && denom[0] == 1 ) {
			type = DICT_INTEGERNUMBER;
			number = nsize;
			where = numer;
		}
		else {
			type = DICT_RATIONALNUMBER;
			number = w[0];
			where = w;
		}
	}
	else {
		s = w + w[0]-1;
		if ( s[0] != 3 || s[-1] != 1 || s[-2] != 1 ) {
Compositeness:;
			MesPrint("& Currently no composite objects allowed in dictionaries.");
			return(1);
		}
		if ( w[0] != w[2]+4 ) goto Compositeness;
		s = w+1;
		switch ( *s ) {
			case SYMBOL:
				if ( s[1] != 4 || s[3] != 1 ) goto Compositeness;
				type = DICT_SYMBOL;
				number = 1;
				where = s+2;
				break;
			case INDEX:
				if ( s[1] != 3 ) goto Compositeness;
				if ( s[2] < 0 ) type = DICT_VECTOR;
				else            type = DICT_INDEX;
				number = 1;
				where = s;
				break;
			default:
				if ( *s < FUNCTION ) {
					MesPrint("& Illegal object in dictionary.");
					return(1);
				}
				if ( s[1] == FUNHEAD ) {
					type = DICT_FUNCTION;
					number = s[0];
				}
				else {
					type = DICT_FUNCTION_WITH_ARGUMENTS;
					number = s[1];
					where = s;
				}
				break;
		}
	}
TestDouble:;
#ifdef WITHREPLACERATHERTHANPUTONTOP
/*
	Test whether we have this element already. If so, replace.
*/
	switch ( type ) {
		case DICT_INTEGERNUMBER:
		case DICT_RATIONALNUMBER:
			if ( dict->numbers <= 0 ) { dict->numbers++; break; }
			ww = &(dict->numbers);
CheckArray:;
			for ( i = 0; i < dict->numelements; i++ ) {
				int j;
				if ( dict->elements[i]->type == type
					&& dict->elements[i]->lhs[0] == number ) {
					for ( j = 0; j < number; j++ ) {
						if ( dict->elements[i]->lhs[j+1] != where[j] ) break;
					}
					if ( j >= number ) {
						M_free(dict->elements[i],"Dictionary element");
						for ( i++; i < dict->numelements; i++ )
							dict->elements[i-1] = dict->elements[i];
						dict->numelements--;
						break;
					}
				}
			}
			(*ww)++;
			break;
		case DICT_SYMBOL:
		case DICT_VECTOR:
		case DICT_INDEX:
		case DICT_FUNCTION:
  			if ( dict->variables <= 0 ) { dict->variables++; break; }
			ww = &(dict->variables);
CheckOne:;
			for ( i = 0; i < dict->numelements; i++ ) {
				if ( dict->elements[i]->type == type
					&& dict->elements[i]->lhs[0] == where[0] ) {
					M_free(dict->elements[i],"Dictionary element");
					for ( i++; i < dict->numelements; i++ )
						dict->elements[i-1] = dict->elements[i];
					dict->numelements--;
					break;
				}
			}
			(*ww)++;
  			break;
		case DICT_FUNCTION_WITH_ARGUMENTS:
  			if ( dict->funwith <= 0 ) { dict->funwith++; break; }
			ww = &(dict->funwith);
			goto CheckArray;
		case DICT_SPECIALCHARACTER:
  			if ( dict->characters <= 0 ) { dict->characters++; break; }
			ww = &(dict->characters);
			goto CheckOne;
	}
#endif
/*
	Create a new element
*/
	if ( dict->numelements >= dict->sizeelements ) {
		DICTIONARY_ELEMENT **d;
		if ( dict->sizeelements <= 0 ) dict->sizeelements = 10;
		else                           dict->sizeelements *= 2;
		d = (DICTIONARY_ELEMENT **)Malloc1(
			sizeof(DICTIONARY_ELEMENT *)*dict->sizeelements,"Dictionary elements");
		for ( i = 0; i < dict->numelements; i++ )
			d[i] = dict->elements[i];
		if ( dict->elements ) M_free(dict->elements,"Dictionary elements");
		dict->elements = d;
	}
	sizelhs = number+1;
	sizerhs = 1; r = right; while ( *r++ ) sizerhs++;
	sizerhs = (sizerhs+sizeof(WORD)-1)/sizeof(WORD)+1;
	new = (DICTIONARY_ELEMENT *)Malloc1(sizeof(DICTIONARY_ELEMENT)
				+sizeof(WORD)*(sizelhs+sizerhs),"Dictionary element");
	new->lhs = (WORD *)(new+1);
	new->rhs = new->lhs+sizelhs;
	new->type = type;
	new->size = number;
	for ( i = 0; i < number; i++ ) new->lhs[i] = where[i];
	new->lhs[i] = 0;
	r = (UBYTE *)(new->rhs);
	while ( *right ) *r++ = *right++;
	*r = 0;

	dict->elements[dict->numelements++] = new;

	switch ( type ) {
		case DICT_INTEGERNUMBER:
		case DICT_RATIONALNUMBER:
			dict->numbers++; break;
		case DICT_SYMBOL:
		case DICT_VECTOR:
		case DICT_INDEX:
		case DICT_FUNCTION:
  			dict->variables++; break;
		case DICT_FUNCTION_WITH_ARGUMENTS:
  			dict->funwith++; break;
		case DICT_SPECIALCHARACTER:
  			dict->characters++; break;
	}

	AT.WorkPointer = OldWork;
	return(0);
}

/*
  	#] AddToDictionary : 
  	#[ SelectDictionary :
*/

int SelectDictionary(UBYTE *name,UBYTE *options)
{
	DICTIONARY *dict;
	int i;
	for ( i = 0; i < AO.NumDictionaries; i++ ) {
		if ( StrCmp(AO.Dictionaries[i]->name,name) == 0 ) {
			dict = AO.Dictionaries[i];
			AO.CurrentDictionary = i+1;
			if ( SetDictionaryOptions(options) < 0 ) {
				AO.CurrentDictionary = 0;
				return(-1);
			}
			else {	/* Now test whether what is requested is really there? */
				return(0);
			}
		}
	}
	MesPrint("@There is no dictionary with the name %s",name);
	exit(-1);
}

/*
  	#] SelectDictionary : 
  	#[ SetDictionaryOptions :
*/

int SetDictionaryOptions(UBYTE *options)
{
	UBYTE *opt, *s, c;
	int retval = 0;
	s = options;
	AO.CurDictNumbers = DICT_ALLNUMBERS;
	AO.CurDictVariables = DICT_DOVARIABLES;
	AO.CurDictSpecials = DICT_DOSPECIALS;
	AO.CurDictFunWithArgs = DICT_DOFUNWITHARGS;
	AO.CurDictNumberWarning = 0;
	while ( *s ) {
		opt = s;
		while ( *s && *s != ',' && *s != ' ' ) s++;
		c = *s; *s = 0;
		if ( StrICmp(opt,(UBYTE *)"nonumbers") == 0 ) {
			AO.CurDictNumbers = DICT_NONUMBERS;
		}
		else if ( StrICmp(opt,(UBYTE *)"integersonly") == 0 ) {
			AO.CurDictNumbers = DICT_INTEGERONLY;
		}
		else if ( StrICmp(opt,(UBYTE *)"rationalsonly") == 0 ) {
			AO.CurDictNumbers = DICT_RATIONALONLY;
		}
		else if ( StrICmp(opt,(UBYTE *)"allnumbers") == 0 ) {
			AO.CurDictNumbers = DICT_ALLNUMBERS;
		}
		else if ( StrICmp(opt,(UBYTE *)"novariables") == 0 ) {
			AO.CurDictVariables = DICT_NOVARIABLES;
		}
		else if ( StrICmp(opt,(UBYTE *)"numbersonly") == 0 ) {
			AO.CurDictNumbers = DICT_ALLNUMBERS;
			AO.CurDictVariables = DICT_NOVARIABLES;
			AO.CurDictSpecials = DICT_NOSPECIALS;
			AO.CurDictFunWithArgs = DICT_NOFUNWITHARGS;
		}
		else if ( StrICmp(opt,(UBYTE *)"variablesonly") == 0 ) {
			AO.CurDictNumbers = DICT_NONUMBERS;
			AO.CurDictVariables = DICT_DOVARIABLES;
			AO.CurDictSpecials = DICT_NOSPECIALS;
			AO.CurDictFunWithArgs = DICT_NOFUNWITHARGS;
		}
		else if ( StrICmp(opt,(UBYTE *)"nospecials") == 0 ) {
			AO.CurDictSpecials = DICT_NOSPECIALS;
		}
		else if ( StrICmp(opt,(UBYTE *)"specialsonly") == 0 ) {
			AO.CurDictNumbers = DICT_NONUMBERS;
			AO.CurDictVariables = DICT_NOVARIABLES;
			AO.CurDictSpecials = DICT_DOSPECIALS;
			AO.CurDictFunWithArgs = DICT_NOFUNWITHARGS;
		}
		else if ( StrICmp(opt,(UBYTE *)"nofunwithargs") == 0 ) {
			AO.CurDictFunWithArgs = DICT_NOFUNWITHARGS;
		}
		else if ( StrICmp(opt,(UBYTE *)"funwithargsonly") == 0 ) {
			AO.CurDictNumbers = DICT_NONUMBERS;
			AO.CurDictVariables = DICT_NOVARIABLES;
			AO.CurDictSpecials = DICT_NOSPECIALS;
			AO.CurDictFunWithArgs = DICT_DOFUNWITHARGS;
		}
		else if ( StrICmp(opt,(UBYTE *)"numberwarning") == 0
		       || StrICmp(opt,(UBYTE *)"warning") == 0 ) {
			AO.CurDictNumberWarning = 1;
		}
		else {
			MesPrint("@ Unrecognized option in %#SetDictionary: %s",opt);
			retval = -1;
		}
		*s = c;
		if ( c == ',' ) s++;
	}
	return(retval);
}

/*
  	#] SetDictionaryOptions : 
  	#[ UnSetDictionary :
*/

void UnSetDictionary(VOID)
{
	AO.CurrentDictionary = 0;
	AO.CurDictNumbers = -1;
	AO.CurDictVariables = -1;
	AO.CurDictSpecials = -1;
	AO.CurDictFunWithArgs = -1;
}

/*
  	#] UnSetDictionary : 
  	#[ RemoveDictionary :

	Mostly needed for .clear
*/

void RemoveDictionary(DICTIONARY *dict)
{
	int i, ifound;
	if ( dict == 0 ) return;
	for ( i = 0; i < AO.NumDictionaries; i++ ) {
		if ( AO.Dictionaries[i] == dict ) {
			ifound = i;
			for (i++; i < AO.NumDictionaries; i++ ) {
				AO.Dictionaries[i-1] = AO.Dictionaries[i];
			}
			AO.NumDictionaries--;
			goto removeit;
		}
	}
	MesPrint("@ Dictionary not found in RemoveDictionary");
	exit(-1);
removeit:;
	for ( i = 0; i < dict->numelements; i++ )
			M_free(dict->elements[i],"Dictionary element");
	for ( i = 0; i < dict->numelements; i++ ) dict->elements[i] = 0;
	if ( dict->elements ) M_free(dict->elements,"Dictionary elements");
	if ( dict->name ) {
		M_free(dict->name,"DictionaryName");
		dict->name = 0;
	}
	dict->sizeelements = 0;
	dict->numelements = 0;
	dict->numbers = 0;
	dict->variables = 0;
	dict->characters = 0;
	dict->funwith = 0;
	dict->gnumelements = 0;
	dict->reserved = 0;
}

/*
  	#] RemoveDictionary : 
  	#[ ShrinkDictionary :

	To be called after a .store to restore the dictionary to the state
	it had at the last .global
	We do not make the elements array shorter.
*/

void ShrinkDictionary(DICTIONARY *dict)
{
	while ( dict->numelements > dict->gnumelements ) {
		dict->numelements--;
		M_free(dict->elements[dict->numelements],"Dictionary element");
		dict->elements[dict->numelements] = 0;
	}
}

/*
  	#] ShrinkDictionary : 
  	#[ DoPreOpenDictionary :
*/

int DoPreOpenDictionary(UBYTE *s)
{
	UBYTE *name;
	int dict;
	if ( AP.PreSwitchModes[AP.PreSwitchLevel] != EXECUTINGPRESWITCH ) return(0);
	if ( AP.PreIfStack[AP.PreIfLevel] != EXECUTINGIF ) return(0);
	while ( *s == ' ' ) s++;

	name = s; s = SkipAName(s);
	if ( *s != 0 && *s != ';' ) {
		MesPrint("@proper syntax is #opendictionary name");
		return(-1);
	}
	*s = 0;

	if ( AP.OpenDictionary > 0 ) {
		MesPrint("@you cannot nest #opendictionary instructions");
		MesPrint("@dictionary %s is open already",
				AO.Dictionaries[AP.OpenDictionary-1]->name);
		return(-1);
	}
	if ( AO.CurrentDictionary > 0 ) {
		MesPrint("@before opening a dictionary you have to first close the selected dictionary");
		return(-1);
	}
/*
	Do we have this dictionary already?
*/
	dict = FindDictionary(name);
	if ( dict == 0 ) dict = AddDictionary(name);
	AP.OpenDictionary = dict;
	return(0);
}

/*
  	#] DoPreOpenDictionary : 
  	#[ DoPreCloseDictionary :
*/

int DoPreCloseDictionary(UBYTE *s)
{
	if ( AP.PreSwitchModes[AP.PreSwitchLevel] != EXECUTINGPRESWITCH ) return(0);
	if ( AP.PreIfStack[AP.PreIfLevel] != EXECUTINGIF ) return(0);
	while ( *s == ' ' ) s++;

	if ( AP.OpenDictionary == 0 && AO.CurrentDictionary == 0 ) {
		MesPrint("@you have neither an open, nor a selected dictionary");
		return(-1);
	}

	AP.OpenDictionary = 0;
	AO.CurrentDictionary = 0;

	return(0);
}

/*
  	#] DoPreCloseDictionary : 
  	#[ DoPreSelectDictionary :
*/

int DoPreSelectDictionary(UBYTE *s)
{
	UBYTE *options, c, *ss, *sss, cc, *name;
	if ( AP.PreSwitchModes[AP.PreSwitchLevel] != EXECUTINGPRESWITCH ) return(0);
	if ( AP.PreIfStack[AP.PreIfLevel] != EXECUTINGIF ) return(0);
	while ( *s == ' ' ) s++;

	if ( AP.OpenDictionary > 0 ) {
		MesPrint("@before selecting a dictionary you have to first close the open dictionary");
		return(-1);
	}

	name = s; s = SkipAName(s);
	ss = s; while ( *s && *s != '(' ) s++;
	c = *ss; *ss = 0;
	if ( c == 0 ) {
		options = ss;
	}
	else {
		options = s+1; SKIPBRA3(s)
		if ( *s != ')' ) {
			MesPrint("@Irregular end of %#SelectDictionary instruction");
			return(-1);
		}
		sss = s;
		cc = *s++; while ( *s == ' ' || *s == '\t' || *s == ';' ) s++;
		*sss = 0;
		if ( *s ) {
			MesPrint("@Irregular end of %#SelectDictionary instruction");
			return(-1);
		}
	}
	return(SelectDictionary(name,options));
}

/*
  	#] DoPreSelectDictionary : 
  	#[ DoPreAdd :

	Syntax:
		#add left :right
        #add left : "right"
	Adds to the currently open dictionary
*/

int DoPreAdd(UBYTE *s)
{
	UBYTE *left, *right;

	if ( AP.PreSwitchModes[AP.PreSwitchLevel] != EXECUTINGPRESWITCH ) return(0);
	if ( AP.PreIfStack[AP.PreIfLevel] != EXECUTINGIF ) return(0);
	while ( *s == ' ' ) s++;

	if ( AP.OpenDictionary == 0 ) {
		MesPrint("@there is no open dictionary to add to");
		return(-1);
	}
/*
	Scan to the : and mark the left and right parts.
*/
	left = s;
	while ( *s && *s != ':' ) {
		if ( *s == '[' ) { SKIPBRA1(s) s++; }
		else if ( *s == '{' ) { SKIPBRA2(s) s++; }
		else if ( *s == '(' ) { SKIPBRA3(s) s++; }
		else if ( *s == ']' || *s == '}' || *s == ')' ) {
			MesPrint("@unmatched brackets in #add instruction");
			return(-1);
		}
		else s++;
	}
	if ( *s == 0 ) {
		MesPrint("@Missing : in #add instruction");
		return(-1);
	}
	*s++ = 0;
	right = s;
	while ( *s == ' ' || *s == '\t' ) s++;
	if ( *s == '"' && s[1] ) {
		right = s+1;
		s = s+2;
		while ( *s ) s++;
		while ( s[-1] != '"' ) s--;
		if ( s <= right ) {
			MesPrint("@Irregular use of double quotes in #add instruction");
			return(-1);
		}
		s[-1] = 0;
	}
	return(AddToDictionary(AO.Dictionaries[AP.OpenDictionary-1],left,right));
}

/*
  	#] DoPreAdd : 
  	#[ DictToBytes :
*/

LONG DictToBytes(DICTIONARY *dict,UBYTE *buf)
{
	int numelements = dict->numelements, sizeelement, i, j, x;
	UBYTE *s1, *s2 = buf;
	DICTIONARY_ELEMENT *e;
/*
	First copy the struct
*/
	s1 = (UBYTE *)dict; j = sizeof(DICTIONARY);
	NCOPY(s2,s1,j)
/*
	Now the elements. Put a size indicator in front of each of them.
*/
	for ( i = 0; i < numelements; i++ ) {
		e = dict->elements[i];
		sizeelement = sizeof(DICTIONARY_ELEMENT)+(e->size+1)*sizeof(WORD);
		s1 = (UBYTE *)e->rhs; x = 0;
		while ( *s1 ) { s1++; x++; }
		x /= sizeof(WORD);
		sizeelement += (x+1) * sizeof(WORD);
		s1 = (UBYTE *)(&sizeelement); j = sizeof(WORD); NCOPY(s2,s1,j)
		s1 = (UBYTE *)e; j = sizeof(DICTIONARY_ELEMENT); NCOPY(s2,s1,j)
		s1 = (UBYTE *)e->lhs; j = (e->size+1)*(sizeof(WORD)); NCOPY(s2,s1,j)
		s1 = (UBYTE *)e->rhs; j = (x+1)*(sizeof(WORD)); NCOPY(s2,s1,j)
	}
	return(s2-buf);
}

/*
  	#] DictToBytes : 
  	#[ DictFromBytes :
*/

DICTIONARY *DictFromBytes(UBYTE *buf)
{
	DICTIONARY *dict = Malloc1(sizeof(DICTIONARY),"Dictionary");
	UBYTE *s1, *s2;
	int i, j, sizeelement;
	DICTIONARY_ELEMENT *e;
/*
	First read the dictionary itself
*/
	s1 = buf;
	s2 = (UBYTE *)dict; j = sizeof(DICTIONARY); NCOPY(s2,s1,j)
/*
	Allocate the elements array:
*/
	dict->elements = (DICTIONARY_ELEMENT **)Malloc1(
			sizeof(DICTIONARY_ELEMENT *)*dict->sizeelements,"dictionary elements");
	for ( i = 0; i < dict->numelements; i++ ) {
		s2 = (UBYTE *)(&sizeelement); j = sizeof(WORD); NCOPY(s2,s1,j)
		e = (DICTIONARY_ELEMENT *)Malloc1(sizeelement*sizeof(UBYTE),"dictionary element");
		dict->elements[i] = e;
		j = sizeelement; s2 = (UBYTE *)e; NCOPY(s2,s1,j)
		e->lhs = (WORD *)(e+1);
		e->rhs = e->lhs + e->size+1;
	}
	return(dict);
}

/*
  	#] DictFromBytes : 
*/
