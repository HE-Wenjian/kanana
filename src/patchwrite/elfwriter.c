/*
  File: elfwriter.c
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: routines for building an elf file for a patch-> Not thread safe
*/

#include "elfwriter.h"
#include <assert.h>
#include <fcntl.h>

//Elf_Data* patch_syms_rel_data=NULL;
//Elf_Data* patch_syms_new_data=NULL;
Elf_Data* patch_rules_data=NULL;
Elf_Data* patch_expr_data=NULL;
Elf_Data* strtab_data=NULL;
Elf_Data* symtab_data=NULL;
Elf_Data* text_data=NULL;//for storing new versions of functions
Elf_Data* rodata_data=NULL;
Elf_Data* rela_text_data=NULL;
//Elf_Data* old_symtab_data=NULL;

//Elf_Scn* patch_syms_rel_scn=NULL;
//Elf_Scn* patch_syms_new_scn=NULL;

Elf_Scn* patch_rules_scn=NULL;
Elf_Scn* patch_expr_scn=NULL;
Elf_Scn* strtab_scn=NULL;
Elf_Scn* symtab_scn=NULL;
Elf_Scn* text_scn=NULL;//for storing new versions of functions
Elf_Scn* rodata_scn=NULL;//hold the new rodata from the patch
Elf_Scn* rela_text_scn=NULL;
//now not writing old symbols b/c
//better to get the from /proc/PID/exe
/*Elf_Scn* old_symtab_scn=NULL;//relevant symbols from the symbol table of the old binary
                             //we must store this in the patch object in case the object
                             //in memory does not have a symbol table loaded in memory
                             */

//todo: use smth like this in the future rather than having a separate variable for everything
//currently this isn't used for everything
typedef struct
{
  Elf_Scn* scn;
  Elf_Data* data;
  //size_t allocatedSize;//bytes allocated for data->d_buf
} ScnInProgress;

ScnInProgress scnInfo[ERS_CNT];


ElfInfo* patch=NULL;
Elf* outelf;

void addDataToScn(Elf_Data* dataDest,void* data,int size)
{
  dataDest->d_buf=realloc(dataDest->d_buf,dataDest->d_size+size);
  MALLOC_CHECK(dataDest->d_buf);
  memcpy((byte*)dataDest->d_buf+dataDest->d_size,data,size);
  dataDest->d_size=dataDest->d_size+size;
}

//adds an entry to the string table, return its offset
int addStrtabEntry(char* str)
{
  int len=strlen(str)+1;
  addDataToScn(strtab_data,str,len);
  elf_flagdata(strtab_data,ELF_C_SET,ELF_F_DIRTY);
  return strtab_data->d_size-len;
}

//return index of entry in symbol table
int addSymtabEntry(Elf_Data* data,Elf32_Sym* sym)
{
  int len=sizeof(Elf32_Sym);
  addDataToScn(data,sym,len);
  if(data==symtab_data)
  {
    patch->symTabCount=data->d_off/len;
  }
  return (data->d_size-len)/len;
}

void createSections(Elf* outelf)
{
  //first create the string table
  strtab_scn=elf_newscn(outelf);
  strtab_data=elf_newdata(strtab_scn);
  strtab_data->d_align=1;
  strtab_data->d_buf=NULL;
  strtab_data->d_off=0;
  strtab_data->d_size=0;
  strtab_data->d_version=EV_CURRENT;
  scnInfo[ERS_STRTAB].scn=strtab_scn;
  scnInfo[ERS_STRTAB].data=strtab_data;
  
  Elf32_Shdr* shdr ;
  shdr=elf32_getshdr(strtab_scn);
  shdr->sh_type=SHT_STRTAB;
  shdr->sh_link=SHN_UNDEF;
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=1;//first real entry in the string table


  
  addStrtabEntry("");//first entry in stringtab null so can have normal unnamed null section
                     //todo: what is the purpose of this?
  addStrtabEntry(".strtab");

  /*
  //now create the patch syms to relocate
  patch_syms_rel_scn=elf_newscn(outelf);
  patch_syms_rel_data=elf_newdata(patch_syms_rel_scn);
  patch_syms_rel_data->d_align=1;
  patch_syms_rel_data->d_buf=NULL;
  patch_syms_rel_data->d_off=0;
  patch_syms_rel_data->d_size=0;//again, will increase as needed
  patch_syms_rel_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_syms_rel_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".patch_syms_rel");

  //now create the patch syms for new variables/functions
  patch_syms_new_scn=elf_newscn(outelf);
  patch_syms_new_data=elf_newdata(patch_syms_new_scn);
  patch_syms_new_data->d_align=1;
  patch_syms_new_data->d_buf=malloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  patch_syms_new_data->d_off=0;
  patch_syms_new_data->d_size=0;
  patch_syms_new_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_syms_new_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".patch_syms_new");*/

  #ifdef RENAMED_DWARF_SCNS
  
  //now create patch rules
  patch_rules_scn=elf_newscn(outelf);
  patch_rules_data=elf_newdata(patch_rules_scn);
  patch_rules_data->d_align=1;
  patch_rules_data->d_buf=NULL;
  patch_rules_data->d_off=0;
  patch_rules_data->d_size=0
  patch_rules_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_rules_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_rules");

  //now create patch expressions
  patch_expr_scn=elf_newscn(outelf);
  patch_expr_data=elf_newdata(patch_expr_scn);
  patch_expr_data->d_align=1;
  patch_expr_data->d_buf=NULL;
  patch_expr_data->d_off=0;
  patch_expr_data->d_size=0;
  patch_expr_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_expr_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_expr");

  #endif

  //ordinary symtab
  symtab_scn=elf_newscn(outelf);
  symtab_data=elf_newdata(symtab_scn);
  symtab_data->d_align=1;
  symtab_data->d_buf=NULL;
  symtab_data->d_off=0;
  symtab_data->d_size=0;
  symtab_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(symtab_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".symtab");

  //first symbol in symtab should be all zeros
  Elf32_Sym sym;
  memset(&sym,0,sizeof(Elf32_Sym));
  addSymtabEntry(symtab_data,&sym);

  //now not using old symtab b/c better to get old symbols from
  // /proc/PID/exe
  /*
  //symtab for symbols we care about in old binary
  old_symtab_scn=elf_newscn(outelf);
  old_symtab_data=elf_newdata(old_symtab_scn);
  old_symtab_data->d_align=1;
  old_symtab_data->d_buf=NULL;
  old_symtab_data->d_off=0;
  old_symtab_data->d_size=0;
  old_symtab_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(old_symtab_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".symtab.v0");
  */

  //text section for new functions
  text_scn=elf_newscn(outelf);
  text_data=elf_newdata(text_scn);
  text_data->d_align=1;
  text_data->d_buf=NULL;
                                       //will be allocced as needed
  text_data->d_off=0;
  text_data->d_size=0;
  text_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(text_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=0;
  shdr->sh_info=0;
  shdr->sh_addralign=1;//normally text is aligned, but we never actually execute from this section
  shdr->sh_name=addStrtabEntry(".text.new");
  shdr->sh_addr=0;//going to have to relocate anyway so no point in trying to keep the same address

  //rodata section for new strings, constants, etc
  //(note that in many cases these may not actually be "new" ones,
  //but unfortunately because .rodata is so unstructured, it can
  //be difficult to determine what is needed and what is not
  rodata_scn=elf_newscn(outelf);
  rodata_data=elf_newdata(rodata_scn);
  rodata_data->d_align=1;
  rodata_data->d_buf=NULL;
  rodata_data->d_off=0;
  rodata_data->d_size=0;
  rodata_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(rodata_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=0;
  shdr->sh_info=0;
  shdr->sh_addralign=1;//normally text is aligned, but we never actually execute from this section
  shdr->sh_name=addStrtabEntry(".rodata.new");

  //rel.text.new
  rela_text_scn=elf_newscn(outelf);
  rela_text_data=elf_newdata(rela_text_scn);
  rela_text_data->d_align=1;
  rela_text_data->d_buf=NULL;
  rela_text_data->d_off=0;
  rela_text_data->d_size=0;
  rela_text_data->d_version=EV_CURRENT;
  shdr=elf32_getshdr(rela_text_scn);
  shdr->sh_type=SHT_RELA;
  shdr->sh_addralign=4;//todo: diff for 64-bit
  shdr->sh_name=addStrtabEntry(".rela.text.new");

  //write symbols for sections
  sym.st_info=ELF32_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_name=elf32_getshdr(text_scn)->sh_name;
  sym.st_shndx=elf_ndxscn(text_scn);
  addSymtabEntry(symtab_data,&sym);
  sym.st_name=elf32_getshdr(strtab_scn)->sh_name;
  sym.st_shndx=elf_ndxscn(strtab_scn);
  addSymtabEntry(symtab_data,&sym);
  sym.st_name=elf32_getshdr(symtab_scn)->sh_name;
  sym.st_shndx=elf_ndxscn(symtab_scn);
  addSymtabEntry(symtab_data,&sym);
  sym.st_name=elf32_getshdr(rela_text_scn)->sh_name;
  sym.st_shndx=elf_ndxscn(rela_text_scn);
  addSymtabEntry(symtab_data,&sym);
  sym.st_name=elf32_getshdr(rodata_scn)->sh_name;
  sym.st_shndx=elf_ndxscn(rodata_scn);
  addSymtabEntry(symtab_data,&sym);

  //fill in some info about the sections we added
  patch->sectionIndices[ERS_TEXT]=elf_ndxscn(text_scn);
  patch->sectionIndices[ERS_SYMTAB]=elf_ndxscn(symtab_scn);
  patch->sectionIndices[ERS_STRTAB]=elf_ndxscn(strtab_scn);
  patch->sectionIndices[ERS_RODATA]=elf_ndxscn(rodata_scn);
  patch->sectionIndices[ERS_RELA_TEXT]=elf_ndxscn(rela_text_scn);
}

//must be called before any other routines
//for each patch object to create
ElfInfo* startPatchElf(char* fname)
{
  patch=zmalloc(sizeof(ElfInfo));
  int outfd = creat(fname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", fname);
  }

  outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
  patch->e=outelf;
  Elf32_Ehdr* ehdr=elf32_newehdr(outelf);
  if(!ehdr)
  {
    death("Unable to create new ehdr\n");
  }
  ehdr->e_ident[EI_MAG0]=ELFMAG0;
  ehdr->e_ident[EI_MAG1]=ELFMAG1;
  ehdr->e_ident[EI_MAG2]=ELFMAG2;
  ehdr->e_ident[EI_MAG3]=ELFMAG3;
  ehdr->e_ident[EI_CLASS]=ELFCLASS32;
  ehdr->e_ident[EI_DATA]=ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION]=EV_CURRENT;
  ehdr->e_ident[EI_OSABI]=ELFOSABI_NONE;
  ehdr->e_machine=EM_386;
  ehdr->e_type=ET_NONE;//not relocatable, or executable, or shared object, or core, etc
  ehdr->e_version=EV_CURRENT;

  createSections(outelf);
  ehdr->e_shstrndx=elf_ndxscn(strtab_scn);//set strtab in elf header
  //todo: perhaps the two string tables should be separate
  patch->strTblIdx=patch->sectionHdrStrTblIdx=ehdr->e_shstrndx;
  return patch;
}



void finalizeDataSize(Elf_Scn* scn,Elf_Data* data)
{
  Elf32_Shdr* shdr=elf32_getshdr(scn);
  shdr->sh_size=data->d_size;
  printf("finalizing data size to %i for section with index %i\n",shdr->sh_size,elf_ndxscn(scn));
}

void finalizeDataSizes()
{
  finalizeDataSize(strtab_scn,strtab_data);
  #ifdef RENAMED_DWARF_SCNS
  
  //then patch expressions
  finalizeDataSize(patch_expr_scn,patch_expr_data);
  #endif

  //ordinary symtab
  finalizeDataSize(symtab_scn,symtab_data);
  //now not using old symtab b/c better to get old symbols from
  // /proc/PID/exe
  /*
  //symtab from old binary

  shdr=elf32_getshdr(old_symtab_scn);
  shdr->sh_size=old_symtab_data->d_size;
  */

  finalizeDataSize(text_scn,text_data);
  finalizeDataSize(rela_text_scn,rela_text_data);
  finalizeDataSize(rodata_scn,rodata_data);
}

void endPatchElf()
{
  #ifdef legacy
  finalizeDataSizes();
  #endif
  if(elf_update (outelf, ELF_C_WRITE) <0)
  {
    fprintf(stderr,"Failed to write out elf file: %s\n",elf_errmsg (-1));
    exit(1);
  }
  endELF(patch);
  patch_rules_data=patch_expr_data=strtab_data=text_data=rodata_data=rela_text_data=NULL;
  patch_rules_scn=patch_expr_scn=strtab_scn=text_scn=rodata_scn=rela_text_scn=NULL;
}

int reindexSectionForPatch(ElfInfo* e,int scnIdx)
{
  char* scnName=getSectionNameFromIdx(e,scnIdx);
  if(!strcmp(".rodata",scnName))
  {
    return elf_ndxscn(rodata_scn);
  }
  else
  {
    death("don't yet support functions referring to sections other than rodata\n");
  }
  return STN_UNDEF;
}

int dwarfWriteSectionCallback(char* name,int size,Dwarf_Unsigned type,
                              Dwarf_Unsigned flags,Dwarf_Unsigned link,
                              Dwarf_Unsigned info,int* sectNameIdx,int* error)
{
  //look through all the sections for one which matches to
  //see if we've already created this section
  Elf_Scn* scn=elf_nextscn(outelf,NULL);
  int nameLen=strlen(name);
  Elf_Data* symtab_data=getDataByERS(patch,ERS_SYMTAB);
  Elf_Data* strtab_data=getDataByERS(patch,ERS_STRTAB);
  for(;scn;scn=elf_nextscn(outelf,scn))
  {
    Elf32_Shdr* shdr=elf32_getshdr(scn);
    if(!strncmp(strtab_data->d_buf+shdr->sh_name,name,nameLen))
    {
      //ok, we found the section we want, now have to find its symbol
      int symtabSize=symtab_data->d_size;
      for(int i=0;i<symtabSize/sizeof(Elf32_Sym);i++)
      {
        Elf32_Sym* sym=(Elf32_Sym*)(symtab_data->d_buf+i*sizeof(Elf32_Sym));
        int idx=elf_ndxscn(scn);
        //printf("we're on section with index %i and symbol for index %i\n",idx,sym->st_shndx);
        if(STT_SECTION==ELF32_ST_TYPE(sym->st_info) && idx==sym->st_shndx)
        {
          *sectNameIdx=i;
          return idx;
        }
      }
      fprintf(stderr,"finding existing section for %s\n",name);
      death("found section already existing but had no symbol, this should be impossible\n");
    }
  }

  //section doesn't already exist, create it
 
  //todo: write this section
  scn=elf_newscn(outelf);
  Elf32_Shdr* shdr=elf32_getshdr(scn);
  shdr->sh_name=addStrtabEntry(name);
  shdr->sh_type=type;
  shdr->sh_flags=flags;
  shdr->sh_size=size;
  shdr->sh_link=link;
  if(0==link && SHT_REL==type)
  {
    //make symtab the link
    shdr->sh_link=elf_ndxscn(symtab_scn);
  }
  shdr->sh_info=info;
  Elf32_Sym sym;
  sym.st_name=shdr->sh_name;
  sym.st_value=0;//don't yet know where this symbol will end up. todo: fix this, so relocations can theoretically be done
  sym.st_size=0;
  sym.st_info=ELF32_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_other=0;
  sym.st_shndx=elf_ndxscn(scn);
  *sectNameIdx=addSymtabEntry(symtab_data,&sym);
  printf("symbol index is %i\n",*sectNameIdx);
  *error=0;  
  return sym.st_shndx;
}