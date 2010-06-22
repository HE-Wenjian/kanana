/*
  File: dwarf_instr.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
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

  Project: Katana
  Date: January 2010
  Description: Functions for manipulating dwarf instructions
*/

#ifndef dwarf_instr_h
#define dwarf_instr_h

#include <stdbool.h>
#include "util/util.h"
#include <libdwarf.h>
#include <dwarf.h>
#include "register.h"

//return value should be freed when you're finished with it
byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,usint* numBytesOut);

//return value should be freed when you're finished with it
byte* decodeLEB128(byte* bytes,bool signed_,usint* numBytesOut,usint* numSeptetsRead);

uint leb128ToUInt(byte* bytes,usint* outLEBBytesRead);

word_t leb128ToUWord(byte* bytes,usint* outLEBBytesRead);


typedef struct
{
  byte* instrs;
  uint numBytes;
  int allocated;
} DwarfInstructions;



typedef struct
{
  short opcode;
  byte* arg1Bytes;//LEB128 number or DW_FORM_block
  usint arg1NumBytes;
  byte* arg2Bytes;
  usint arg2NumBytes;
  int arg1;
  int arg2;
  byte* arg3Bytes;
  usint arg3NumBytes;
  int arg3;
  //both the integer and bytes values for an argument are not valid at any one time
  //which is valid depends on the opcode
} DwarfInstruction;

typedef struct
{
  int type;//one of DW_CFA_
  int arg1;
  PoReg arg1Reg;//whether used depends on the type
  word_t arg2;//whether used depends on the type
  PoReg arg2Reg;//whether used depends on the type
  word_t arg3;//used only for DW_CFA_KATANA_do_fixups
} RegInstruction;


//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr);

void printInstruction(RegInstruction inst);

//some versions of dwarf.h have a misspelling
#if !DW_CFA_lo_user
#define DW_CFA_lo_user DW_CFA_low_user
#endif
//specifies that the value for the given register should be computed
//by applying the fixups in the given FDE It therefore takes three
//LEB128 operands, the first is a register number to be assigned, the
//second is a register to use as the "CURR_TARG_OLD" during the fixup,
//and the third is an FDE number (index of the FDE to use for fixups)
#define DW_CFA_KATANA_fixups DW_CFA_lo_user+0x5

//same thing except it is understood that the register to be assigned actually
//corresponds to a pointer to a type given by the index of the FDE
#define DW_CFA_KATANA_fixups_pointer DW_CFA_lo_user+0x6

#endif
