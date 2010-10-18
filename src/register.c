/*
  File: register.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: January 2010
  Description: registers in the patching VM. Exploitation of the fact that Dwarf
               "registers" are a very flexible concept
*/

#include "register.h"
#include "dwarf_instr.h"
#include <assert.h>
#include "patcher/target.h"
#include "symbol.h"
#include <stdio.h>
#include "leb.h"

PoReg readRegFromLEB128(byte* leb,usint* bytesRead)
{
  PoReg result;
  usint numBytes;
  usint numSeptets;
  byte* bytes=decodeLEB128(leb,false,&numBytes,&numSeptets);
  *bytesRead=numSeptets;
  assert(numBytes>0);
  if(bytes[0] < ERT_PO_START)
  {
    memset(&result,0,sizeof(result));
    result.type=ERT_BASIC;
    assert(numBytes<sizeof(idx_t));
    memcpy(&result.u.index,bytes,numBytes);
    return result;
  }
  result.type=bytes[0];
  assert(result.type>ERT_PO_START && result.type<ERT_PO_END);
  switch(result.type)
  {
  case ERT_CURR_TARG_NEW:
  case ERT_CURR_TARG_OLD:
    assert(numBytes>=1+sizeof(int) && numBytes<=1+2*sizeof(int));
    memcpy(&result.size,bytes+1,sizeof(int));
    result.u.offset=0;
    memcpy(&result.u.offset,bytes+1+sizeof(int),min(numBytes-1-sizeof(int),sizeof(int)));
    break;
  case ERT_EXPR:
    //todo: I forget what I was doing here
    assert(5==numBytes);//todo: diff for 64bit
    result.size=0;//not used
    memcpy(&result.u.offset,bytes+1,4);
    break;
  case ERT_NEW_SYM_VAL:
  case ERT_OLD_SYM_VAL:
    assert(5==numBytes);//todo: diff for 64bit
    result.size=0;//not used
    memcpy(&result.u.offset,bytes+1,4);
    break;
  default:
    death("unsupported register type\n");
  }
  free(bytes);
  return result;
}

//the returned string should be freed
char* strForReg(PoReg reg)
{
  char* buf=zmalloc(128);
  assert(reg.type!=ERT_NONE);
  switch(reg.type)
  {
  case ERT_CURR_TARG_OLD:
    snprintf(buf,128,"{CURR_TARG_OLD,0x%x bytes,0x%x off}",(unsigned int)reg.size,reg.u.offset);
    break;
  case ERT_CURR_TARG_NEW:
    snprintf(buf,128,"{CURR_TARG_NEW,0x%x bytes,0x%x off}",(unsigned int)reg.size,reg.u.offset);
    break;
  case ERT_EXPR:
    //todo: print out expression
    snprintf(buf,128,"{EXPR,0x%x off",reg.u.offset);
    break;
  case ERT_NEW_SYM_VAL:
    //todo: find symbol name
    snprintf(buf,128,"{NEW_SYM_VAL,idx %i}",reg.u.index);
    break;
  case ERT_OLD_SYM_VAL:
    //todo: find symbol name
    snprintf(buf,128,"{OLD_SYM_VAL,idx %i}",reg.u.index);
    break;
  case ERT_CFA:
    snprintf(buf,128,"{CFA}");
    break;
  case ERT_BASIC:
    snprintf(buf,128,"r%i",reg.u.index);
    break;
  default:
    death("unsupported register type\n");
  }
  return buf;
}

void printReg(PoReg reg,FILE* f)
{
  char* str=strForReg(reg);
  fprintf(f,"%s",str);
  free(str);
}

//resolve any register to a value (as distinct from the symbolic form it may be in)
//this may include resolving symbols in elf files, dereferencing
//things in memory, etc
//the result will be written to the result parameter and the number
//of bytes in the result will be returned;
//some values behave differently if they're being assigned than evaluated
//flags determines this behaviour (should be OR'd E_REG_RESOLVE_FLAGS
int resolveRegisterValue(PoReg* reg,SpecialRegsState* state,byte** result,int flags)
{
  //todo: error on value of NEW things if not right hand?
  //perhaps don't even need the rightHand parameter
  addr_t addr=0;
  switch(reg->type)
  {
  case ERT_NONE:
    death("resolveRegisterValue cannot be called on a register of NONE type\n");
    break;
  case ERT_CURR_TARG_NEW:
    addr=state->currAddrNew+reg->u.offset;
    break;
  case ERT_CURR_TARG_OLD:
    addr=state->currAddrOld+reg->u.offset;
    if(flags&ERRF_ASSIGN)
    {
      death("Cannot assign a memory value in the old version of the target\n");
    }
    break;
  case ERT_EXPR:
    //todo: implement these
    death("expressions not supported yet. Poke the developer\n");
    break;
  case ERT_OLD_SYM_VAL:
    if(flags&ERRF_ASSIGN)
    {
      death("Cannot assign a symbol  in the old version of the target\n");
    }
    addr=getSymAddress(state->oldBinaryElf,reg->u.index);
    *result=zmalloc(sizeof(addr_t));
    memcpy(*result,&addr,sizeof(addr_t));
    return sizeof(addr_t);
    break;
  case ERT_NEW_SYM_VAL:
    //todo: implement these
    death("ERT_NEW_SYM_VAL not supported yet. Poke the developer\n");
    break;
  case ERT_CFA:
    //todo: implement these
    death("ERT_CFA not supported yet. Poke the developer\n");
    break;
  default:
    death("unknown register type\n");
  }
  if(addr)
  {
    if(!(flags & ERRF_DEREFERENCE))
    {
      *result=zmalloc(sizeof(addr_t));
      memcpy(*result,&addr,sizeof(addr_t));
      return sizeof(addr_t);
    }
    else
    {
      //need to dereference an address
      *result=zmalloc(reg->size);
      memcpyFromTarget(*result,addr,reg->size);
      return reg->size;
    }
  }
  death("resolveRegisterValue should have returned by now\n");
  return 0;
}

void printRule(PoRegRule rule,int regnum)
{

  char* regStr=NULL;
  regStr=strForReg(rule.regLH);
  switch(rule.type)
  {
  case ERRT_UNDEF:
    printf("%s = Undefined\n",regStr);
    break;
  case ERRT_OFFSET:
    printf("%s = cfa + %i\n",regStr,rule.offset);
    break;
  case ERRT_REGISTER:
    printf("%s = %s\n",regStr,strForReg(rule.regRH));
    break;
  case ERRT_CFA:
    printf("cfa = %s + %i\n",strForReg(rule.regRH),rule.offset);
    break;
  case ERRT_RECURSE_FIXUP:
    printf("%s = recurse fixup with FDE#%lu based at %s\n",regStr,(unsigned long)rule.index,strForReg(rule.regRH));
    break;
  case ERRT_RECURSE_FIXUP_POINTER:
    printf("%s = recurse fixup pointer with FDE#%lu based at %s\n",regStr,(unsigned long)rule.index,strForReg(rule.regRH));
    break;
  default:
    death("unknown rule type\n");
  }
}

void printRules(Dictionary* rulesDict,char* tabstr)
{
  PoRegRule** rules=(PoRegRule**)dictValues(rulesDict);
  for(int i=0;rules[i];i++)
  {
    if(rules[i]->type!=ERRT_UNDEF)
    {
      printf("%s",tabstr);
      printRule(*rules[i],i);
    }
  }
}
