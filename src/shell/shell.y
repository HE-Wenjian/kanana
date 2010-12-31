%{
#include <cstdlib>
#include "commands/loadCommand.h"
#include "commands/saveCommand.h"
#include "commands/replaceCommand.h"
#include "commands/shellCommand.h"
#include "commands/dwarfscriptCommand.h"
#include "commands/infoCommand.h"
#include "parse_helper.h"


ParseNode rootParseNode;
  
#define YYSTYPE ParseNode
#define YYDEBUG 1

//so we don't get any warnings in the code generated by bison, which
//wasn't originally meant to be compiled as C++
#pragma GCC diagnostic ignored "-Wwrite-strings"

 
extern int yylex();
extern "C"
{
#include "util/dictionary.h"
  int yyerror(char *s);

  extern Dictionary* shellVariables;  
  
  //saved values from lexing
  int lineNumber=1;
  char* savedVarName=NULL;
  char* savedString=NULL;
}


%}

//Token definitions
%token T_LOAD T_SAVE T_TRANSLATE T_REPLACE T_SECTION T_VARIABLE T_STRING_LITERAL
%token T_DATA T_DWARFSCRIPT T_COMPILE T_EMIT T_SHELL_COMMAND
%token T_INFO T_EXCEPTION_HANDLING
%token T_INVALID_TOKEN


%%

root : line_list
{
  $$=$1;
  rootParseNode=$1;
}

line_list : line_list line
{
  CommandList* listItem=(CommandList*)zmalloc(sizeof(CommandList));
  listItem->cmd=$2.u.cmd;
  listItem->next=NULL;
  if(PNT_EMPTY==$1.type)
  {
    $$.type=PNT_LIST;
    listItem->tail=listItem;
    $$.u.listItem=listItem;
  }
  else
  {
    assert(PNT_LIST==$$.type);
    $1.u.listItem->tail->next=listItem;
    $1.u.listItem->tail=listItem;
    $$=$1;
  }
}
|
{
  $$.type=PNT_EMPTY;
}

line : assignment
{
  $$=$1;
}
| commandline
{
  $$=$1;
}

assignment : variable '=' commandline
{
  $3.u.cmd->setOutputVariable($1.u.var);
  $$=$3;
}

commandline : loadcmd {$$=$1;$$.type=PNT_CMD;}
| savecmd {$$=$1;$$.type=PNT_CMD;}
| dwarfscriptcmd {$$=$1;$$.type=PNT_CMD;}
| replacecmd {$$=$1;$$.type=PNT_CMD;}
| shellcmd {$$=$1;$$.type=PNT_CMD;}
| infocmd {$$=$1;$$.type=PNT_CMD;}


loadcmd : T_LOAD param
{
  $$.u.cmd=new LoadCommand($2.u.param);
}

savecmd : T_SAVE param param
{
  $$.u.cmd=new SaveCommand($2.u.param,$3.u.param);
}
| T_SAVE error
{
  fprintf(stderr,"save takes two parameters\n");
  YYERROR;
}

dwarfscriptcmd : T_DWARFSCRIPT T_COMPILE param 
{
  $$.u.cmd=new DwarfscriptCommand(DWOP_COMPILE,$3.u.param,NULL);
}
| T_DWARFSCRIPT T_COMPILE param param
{
  $$.u.cmd=new DwarfscriptCommand(DWOP_COMPILE,$3.u.param,$4.u.param);
}
| T_DWARFSCRIPT T_EMIT param param param
{
  $$.u.cmd=new DwarfscriptCommand(DWOP_EMIT,$3.u.param,$4.u.param,$5.u.param);
}

/* translatecmd : T_TRANSLATE translate_fmt translate_fmt param */
/* { */
/*   $$.u.cmd=new TranslateCommand($2.intval,$3.intval,$4.u.param); */
/* } */
/* //the form that take the output file as a parameter */
/* | T_TRANSLATE translate_ftmt paratranslate_fmt param param */
/* { */
/*   $$.u.cmd=new TranslateCommand($2.intval,$3.intval,$4.u.param,$5.u.param); */
/* } */

/* translate_fmt : T_DATA {$$.u.intval=TFT_DATA} */
/* | T_DWARFSCRIPT {$$.u.intval=TFT_DWARFSCRIPT} */

replacecmd : T_REPLACE T_SECTION param param param
{
  $$.u.cmd=new ReplaceCommand(RT_SECTION,$3.u.param,$4.u.param,$5.u.param);
}

shellcmd : T_SHELL_COMMAND param
{
  $$.u.cmd=new SystemShellCommand($2.u.param);
}

infocmd : T_INFO T_EXCEPTION_HANDLING param param
{
  $$.u.cmd=new InfoCommand(IOP_EXCEPTION,$3.u.param,$4.u.param);
}
| T_INFO T_EXCEPTION_HANDLING param
{
  $$.u.cmd=new InfoCommand(IOP_EXCEPTION,$3.u.param);
}
| T_INFO error
{
  fprintf(stderr,"Unrecognized info usage\n");
  YYERROR;
}

param : variable
{
  $$.u.param=$1.u.var;
  $$.type=PNT_PARAM;
    
}
| stringParam
{
  $$.u.param=new ShellParam($1.u.string);
  $$.type=PNT_PARAM;
}

variable : T_VARIABLE
{
  $$.type=PNT_VAR;
  $$.u.var=(ShellVariable*)dictGet(shellVariables,savedVarName);
  if(!$$.u.var)
  {
    $$.u.var=new ShellVariable(savedVarName);
    dictInsert(shellVariables,savedVarName,$$.u.var);
  }
}

stringParam : T_STRING_LITERAL
{
  $$.u.string=savedString;
  $$.type=PNT_STR;
}


%%
int yyerror(char *s)
{
  fprintf(stderr, "%s at line %d\n", s, lineNumber);
  return 0;
}
