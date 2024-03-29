/*
  File: elfparse.h
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
  Date: January 10
  Description: Read information from an ELF file
*/

#ifndef elfparse_h
#define elfparse_h
#include <libelf.h>
#include <gelf.h>
#include "util/util.h"
#include "types.h"
#include <elf.h>
#include "arch.h"

#include "callFrameInfo.h"

typedef struct ElfInfo
{
  int symTabCount;
  Elf* e;
  size_t sectionHdrStrTblIdx;
  size_t strTblIdx;
  Elf_Data* textRelocData;
  int textRelocCount;
  int sectionIndices[ERS_CNT];
  int dataStart[2];//in memory and on disk
  int textStart[2];//in memory and on disk
  int fd;//file descriptor for elf file
  char* fname;//file name associated with descriptor. Not always set
  DwarfInfo* dwarfInfo;
  CallFrameInfo callFrameInfo;
  bool dataAllocatedByKatana;//used for memory management
  bool isPO;//is this elf object a patch object?
  int arch_bitwidth;
  #ifdef KATANA_X86_64_ARCH
  //set true if text sections use a small code
  //model, requiring any relocations of text, data, rodata, etc
  //to be in the lower 32-bit address space of the program
  bool textUsesSmallCodeModel;
  #endif
} ElfInfo;


ElfInfo* openELFFile(char* fname);
//called when we are finished using the given ELF file
//this function does nothing more than cleanup and deallocate resources.
void endELF(ElfInfo* _e);

//have to pass the name that the elf file will originally get written
//out to, because of the way elf_begin is set up
//if flushToDisk is false doesn't
//actually write to disk right now
ElfInfo* duplicateElf(ElfInfo* e,char* outfname,bool flushToDisk,bool keepLayout);

//write out a copy of this ELF object to the given location on disk.
//if keepLayout is true, don't allow libelf to rearrange the
//layout. ELF files seem to get screwed up sometimes when libelf is
//allowed a free reign. I don't quite understand why.
//return true on success
bool writeOutElf(ElfInfo* e,char* outfname,bool keepLayout);
void findELFSections(ElfInfo* e);


#endif
