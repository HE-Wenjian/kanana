%option noyywrap
%option nounput
%option noinput
 //different prefix to avoid conflicting with our other parser
%option prefix="yydw"
%x POUND_COMMENT
%x IN_STRING
%option header-file="shell/dwarfscript/dwarfscript.yy.h"
%option outfile="shell/dwarfscript/dwarfscript.yy.c"


%top{
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "shell/dwarfscript/dwarfscript.tab.h"
extern int dwLineNumber;
//values saved for the parser
extern char* savedString;
extern int savedRegisterNumber;
extern int savedInt;
extern double savedDouble;
void accumulateText(char* text);
char* finishAccumulateText();
}

POS_INT [1-9][0-9]*
NONNEG_INT [0-9]+
INT       -?[0-9]+
HEX        0[Xx][0-9a-fA-F]+
FLOAT     -?(([0-9]*\.[0-9]+)|([0-9]+\.[0-9]*))
REGISTER  r[0-9]+

%%

"#"        BEGIN(POUND_COMMENT);
"\""        BEGIN(IN_STRING);
"begin" return T_BEGIN;
"end" return T_END;
":" return ':';
"index" return T_INDEX;
"data_align" return T_DATA_ALIGN;
"code_align" return T_CODE_ALIGN;
"return_addr_rule" return T_RET_ADDR_RULE;
"augmentation" return T_AUGMENTATION;
"initial_location" return T_INITIAL_LOCATION;
"address_range" return T_ADDRESS_RANGE;
"length" return T_LENGTH;
"cie_index" return T_CIE_INDEX;
"version" return T_VERSION;
"address_size" return T_ADDRESS_SIZE;
"segment_size" return T_SEGMENT_SIZE;

"FDE"|"fde" return T_FDE;
"CIE"|"cie" return T_CIE;
"INSTRUCTIONS"|"instructions" return T_INSTRUCTIONS;

  /*all of the DWARF instructions*/
"DW_CFA_advance_loc"        return T_DW_CFA_advance_loc;
"DW_CFA_offset"             return T_DW_CFA_offset;
"DW_CFA_restore"            return T_DW_CFA_restore;
"DW_CFA_nop"                return T_DW_CFA_nop;
"DW_CFA_set_loc"            return T_DW_CFA_set_loc;
"DW_CFA_advance_loc1"       return T_DW_CFA_advance_loc1;
"DW_CFA_advance_loc2"       return T_DW_CFA_advance_loc2;
"DW_CFA_advance_loc4"       return T_DW_CFA_advance_loc4;
"DW_CFA_offset_extended"    return T_DW_CFA_offset_extended;
"DW_CFA_restore_extended"   return T_DW_CFA_restore_extended;
"DW_CFA_undefined"          return T_DW_CFA_undefined;
"DW_CFA_same_value"         return T_DW_CFA_same_value;
"DW_CFA_register"           return T_DW_CFA_register;
"DW_CFA_remember_state"     return T_DW_CFA_remember_state;
"DW_CFA_restore_state"      return T_DW_CFA_restore_state;
"DW_CFA_def_cfa"            return T_DW_CFA_def_cfa;
"DW_CFA_def_cfa_register"   return T_DW_CFA_def_cfa_register;
"DW_CFA_def_cfa_offset"     return T_DW_CFA_def_cfa_offset;
"DW_CFA_def_cfa_expression" return T_DW_CFA_def_cfa_expression;
"DW_CFA_expression"         return T_DW_CFA_expression;
"DW_CFA_offset_extended_sf" return T_DW_CFA_offset_extended_sf;
"DW_CFA_def_cfa_sf"         return T_DW_CFA_def_cfa_sf;
"DW_CFA_def_cfa_offset_sf"  return T_DW_CFA_def_cfa_offset_sf;
"DW_CFA_val_offset"         return T_DW_CFA_val_offset;
"DW_CFA_val_offset_sf"      return T_DW_CFA_val_offset_sf;
"DW_CFA_val_expression"     return T_DW_CFA_val_expression;

{POS_INT}   savedInt=atoi(yydwtext); return T_POS_INT;
{NONNEG_INT}   savedInt=atoi(yydwtext); return T_NONNEG_INT;
{INT}   savedInt=atoi(yydwtext); return T_INT;
{HEX}   savedInt=strtol(yydwtext, NULL, 16); return T_NONNEG_INT;
{FLOAT}   savedDouble=strtod(yydwtext,NULL); return T_FLOAT;

{REGISTER} savedRegisterNumber=atoi(yydwtext); return T_REGISTER;

<POUND_COMMENT>{
  [\n]      dwLineNumber++;BEGIN(INITIAL);
  [^\n]+   // eat comment
}

<IN_STRING>{
  "\""        savedString=strdup(finishAccumulateText());BEGIN(INITIAL);return T_STRING_LITERAL;
  [^\"\n\\]*  accumulateText(yydwtext);
  \\\n        dwLineNumber++;//eat newlines
  \\\"        accumulateText(yydwtext);
  \\          accumulateText(yydwtext);
 }
[ \t]+  //eat whitespace

\n  dwLineNumber++;
.   fprintf(stderr,"Encountered invalid token at line %i\n",dwLineNumber);return T_INVALID_TOKEN;

%%

static char* textAccumulator=NULL;
static bool textAccumulating=false;
void accumulateText(char* text)
{
  if (!textAccumulating)
  {
    if (textAccumulator)
    {
      free(textAccumulator);
      textAccumulator = NULL;
    }
    textAccumulating = true;
    textAccumulator = (char*)malloc(2 * strlen(text) + 1);
    strcpy(textAccumulator, text);
  }
  else
  {
    int accumLen = strlen(textAccumulator);
    int newLen = strlen(text);
    textAccumulator = (char*)realloc(textAccumulator, accumLen + newLen + 1);
    if (!textAccumulating)
    {
      abort();
    }
    strcat(textAccumulator, text);
  }
}

char* finishAccumulateText()
{
  textAccumulating = false;
  return textAccumulator;
}