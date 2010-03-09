/*
  File: symbol.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Functions for dealing with symbols in ELF files
*/

#include "symbol.h"
#include <assert.h>
#include "util/util.h"
#include "util/logging.h"
#include <string.h>
#include "patcher/versioning.h"

void getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym)
{
  Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
  if(!gelf_getsym(symTabData,symIdx,outSym))
  {
    death("gelf_getsym failed trying to get symbol at index %i\n",symIdx);
  }
}

addr_t getSymAddress(ElfInfo* e,int symIdx)
{
  Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
  GElf_Sym sym;
  if(!gelf_getsym(symTabData,symIdx,&sym))
  {
    death("gelf_getsym failed in getSymAddress\n");
  }
  return sym.st_value;
}

char* unmangleSymbolName(char* name)
{
  char* symbolNameUnmangled=NULL;
  char* atSignPtr=strchr(name,'@');
  if(atSignPtr)
  {
    symbolNameUnmangled=zmalloc(strlen(name)+1);
    int atSignIdx=atSignPtr-name;
    strcpy(symbolNameUnmangled,name);
    symbolNameUnmangled[atSignIdx]='\0';
  }
  else
  {
    symbolNameUnmangled=strdup(name);
  }
  return symbolNameUnmangled;
}

//find the symbol matching the given symbol
idx_t findSymbol(ElfInfo* e,GElf_Sym* sym,ElfInfo* ref,int flags)
{
  idx_t retval=STN_UNDEF;
  Elf_Data* symTabData=NULL;
  char* (*getstrfunc)(ElfInfo*,int)=NULL;
  if(flags & ESFF_NEW_DYNAMIC)
  {
    symTabData=getDataByERS(e,ERS_DYNSYM);
    getstrfunc=&getDynString;
  }
  else
  {
    symTabData=getDataByERS(e,ERS_SYMTAB);
    getstrfunc=&getString;
  }
  char* symbolName=getString(ref,sym->st_name);//todo not supporting ESFF_OLD_DYNAMIC yet
  char* symbolNameUnmangled=symbolName;
  if(flags & ESFF_MANGLED_OK)
  {
    symbolNameUnmangled=unmangleSymbolName(symbolName);
  }
  char* symbolNameDot=zmalloc(strlen(symbolName)+2);//for --fdata-sections and --ffunction-sections
  strcpy(symbolNameDot,".");
  strcat(symbolNameDot,symbolName);
  //traverse the symbol table to find the symbol we're looking for. Yes this is slow
  //todo: build our own hash table since the .hash section seems incomplete
  //start at 1 because never trying to match symbol 0
   int bind=ELF64_ST_BIND(sym->st_info);
   int type=ELF64_ST_TYPE(sym->st_info);
  int numEntries=symTabData->d_size/sizeof(Elf32_Sym);//todo: diff for 64 bit
  for (int i = 1; i < numEntries; ++i)
  {
    Elf32_Sym sym2;
    //get the symbol in an unsafe manner because
    //we may be getting it from a data buffer we're in the process of filling
    memcpy(&sym2,symTabData->d_buf+i*sizeof(Elf32_Sym),sizeof(Elf32_Sym));
    char* symname=(*getstrfunc)(e, sym2.st_name);
    int bind2=ELF32_ST_BIND(sym2.st_info);
    int type2=ELF32_ST_TYPE(sym2.st_info);
    char* symnameUnmangled=unmangleSymbolName(symname);
    //todo: bug with symbols with names substrings of other names, need to properly unmangle symname
    if( (STT_SECTION==type && STT_SECTION==type2) ||
       (!symname && !symbolName) ||
       (!symname && !strlen(symbolName)) ||
       (!strlen(symname) && !symbolName) ||
       (symname && symbolName && !strcmp(symnameUnmangled,symbolNameUnmangled)))
    {
      free(symnameUnmangled);
      if(symname && strlen(symname))
      {
        logprintf(ELL_INFO_V2,ELS_SYMBOL,"[%i] matches %s, has name %s\n",i,symbolName,symname);
      }
      else
      {
        logprintf(ELL_INFO_V2,ELS_SYMBOL,"[%i] both have no name. They might be the same section\n",i);
      }
      //ok, the right name, but are other things right too?
     
      
      if(bind != bind2)
      {
        logprintf(ELL_INFO_V2,ELS_SYMBOL,"fails on bind\n");
        continue;
      }
      if(type!= STT_NOTYPE && type2!=STT_NOTYPE && type != type2)
      {
        logprintf(ELL_INFO_V2,ELS_SYMBOL,"fails on type\n");
        continue;
      }

      //don't match on size because the size of a variable may
      //have changed
      
      if(sym->st_other!=sym2.st_other)
      {
        logprintf(ELL_INFO_V2,ELS_SYMBOL,"fails on other\n");
        continue;
      }
      //now the hard one to deal with: section index. This is especially
      //important to deal with for section symbols though as there may be no other
      //means of differentiating them
      int shndxRef=sym->st_shndx;
      int shndxNew=sym2.st_shndx;
      //allowing undefined to be a wildcard because may be bringing
      //in a symbol from a relocatable object
      //also allowing imprecise matching on common,
      //because symbols may be common in a .o file and then
      //get put in a section in the fully linked binary
      if(shndxRef!=SHN_UNDEF && shndxNew!=SHN_UNDEF &&
         shndxRef!=SHN_COMMON && shndxNew!=SHN_COMMON)
      {
        Elf_Scn* scnRef=elf_getscn(ref->e,shndxRef);
        Elf_Scn* scnNew=elf_getscn(e->e,shndxNew);
        GElf_Shdr shdrRef;
        GElf_Shdr shdrNew;
        gelf_getshdr(scnRef,&shdrRef);
        gelf_getshdr(scnNew,&shdrNew);
        char* scnNameRef=strdup(getScnHdrString(ref,shdrRef.sh_name));
        char* scnNameNew=strdup(getScnHdrString(e,shdrNew.sh_name));
        //if -fdata-sections or -ffunction-sections is used then
        //we might have issues with section names having the name of the
        //var/function appended, so we strip these
        if(strEndsWith(scnNameRef,symbolNameDot))
        {
          scnNameRef[strlen(scnNameRef)-strlen(symbolNameDot)]='\0';
        }
        if(strEndsWith(scnNameNew,symbolNameDot))
        {
          scnNameNew[strlen(scnNameNew)-strlen(symbolNameDot)]='\0';
        }

        //also strip versioning from the section names if allowed
        if(flags & ESFF_VERSIONED_SECTIONS_OK)
        {
          char* vers=getVersionStringOfPatchSections();
          char* buf=zmalloc(strlen(vers)+2);
          sprintf(buf,".%s",vers);
          if(strEndsWith(scnNameRef,buf))
          {
            scnNameRef[strlen(scnNameRef)-strlen(buf)]='\0';
          }
          if(strEndsWith(scnNameNew,buf))
          {
            scnNameNew[strlen(scnNameNew)-strlen(buf)]='\0';
          }
          free(buf);

        }
        
        //printf("old refers to section name %s and new refers to section name %s\n",scnNameRef,scnNameNew);
        if((scnNameRef && !scnNameNew) || (!scnNameNew && scnNameNew) ||
           (scnNameRef && scnNameNew && strcmp(scnNameRef,scnNameNew)))
        {
          //we might still be saved by considering data and bss to be the same section
          if(type == STT_SECTION ||
             (!(flags & ESFF_BSS_MATCH_DATA_OK) ||
              !((!strncmp(scnNameRef,".data",strlen(".data")) &&
                 !strncmp(scnNameNew,".bss",strlen(".bss"))) ||
                (!strncmp(scnNameRef,".bss",strlen(".bss")) &&
                 !strncmp(scnNameNew,".data",strlen(".data"))))))
          {
            logprintf(ELL_INFO_V2,ELS_SYMBOL,"symbol match fails on section name (%s vs %s)\n",scnNameRef,scnNameNew);
            free(scnNameNew);
            free(scnNameRef);
            continue;
          }
        }
        free(scnNameNew);
        free(scnNameRef);
      }
      logprintf(ELL_INFO_V1,ELS_SYMBOL,"found symbol %s at index %i\n",symbolName,i);
      retval=i;
    }
    else
    {
      free(symnameUnmangled);
    }
  }
  
  if(symbolNameUnmangled!=symbolName)
  {
    free(symbolNameUnmangled);
  }
  free(symbolNameDot);
  return retval;
}

GElf_Sym nativeSymToGELFSym(Elf32_Sym sym)
{
  GElf_Sym res;
  res.st_name=sym.st_name;
  res.st_value=sym.st_value;
  res.st_size=sym.st_size;
  res.st_info=ELF64_ST_INFO(ELF32_ST_BIND(sym.st_info),
                            ELF32_ST_TYPE(sym.st_info));
  res.st_other=sym.st_other;
  res.st_shndx=sym.st_shndx;
  return res;
}


Elf32_Sym gelfSymToNativeSym(GElf_Sym sym)
{
  Elf32_Sym res;
  res.st_name=sym.st_name;
  res.st_value=sym.st_value;
  res.st_size=sym.st_size;
  res.st_info=ELF32_ST_INFO(ELF64_ST_BIND(sym.st_info),
                            ELF64_ST_TYPE(sym.st_info));
  res.st_other=sym.st_other;
  res.st_shndx=sym.st_shndx;
  return res;
}

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return STN_UNDEF if it cannot be found
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx,int flags)
{
  //todo: need a method of apropriately handling local/week symbols
  //(other than section ones, which aren't really a big deal to handle)
  //we should be able to use the FILE symbols placed above local symbols
  //to do this
  
  GElf_Sym sym;
  //todo: in the future we could match on other things beside name
  //in case name was duplicated for some reason
  getSymbol(old,oldIdx,&sym);

  //do it once first without fuzzy matching
  //because always prefer exact match
  int flagsNoFuzzy=flags & ~ESFF_FUZZY_MATCHING_OK;
  idx_t idx=findSymbol(new,&sym,old,flagsNoFuzzy);
  if(STN_UNDEF==idx && flags!=flagsNoFuzzy)
  {
    idx=findSymbol(new,&sym,old,flags);
  }
  if(STN_UNDEF==idx)
  {
    char* symbolName=getString(old,sym.st_name);
    fprintf(stderr,"Symbol '%s' could not be reindexed\n",symbolName);
  }
  return idx;
}

//flags is OR'd E_SYMBOL_FIND_FLAGS
//only ESFF_MANGLED_OK and ESFF_DYNAMIC are relevant
int getSymtabIdx(ElfInfo* e,char* symbolName,int flags)
{
  //todo: need to consider endianness?
  //assert(e->hashTableData);
  assert(e->strTblIdx>0);
  
  //Hash table only contains dynamic symbols, don't use it here
  /*
  int nbucket, nchain;
  memcpy(&nbucket,hashTableData->d_buf,sizeof(Elf32_Word));
  memcpy(&nchain,hashTableData->d_buf+sizeof(Elf32_Word),sizeof(Elf32_Word));
  logprintf(ELL_INFO_V4,ELS_MISC,"there are %i buckets, %i chain entries, and total size of %i\n",nbucket,nchain,hashTableData->d_size);


  unsigned long int hash=elf_hash(symbolName)%nbucket;
  logprintf(ELL_INFO_V4,ELS_MISC,"hash is %lu\n",hash);
  //get the index from the bucket
  int idx;
  memcpy(&idx,hashTableData->d_buf+sizeof(Elf32_Word)*(2+hash),sizeof(Elf32_Word));
  int nextIdx=idx;
  char* symname="";
  while(strcmp(symname,symbolName))
  {
    idx=nextIdx;
    logprintf(ELL_INFO_V4,ELS_MISC,"idx is %i\n",idx);
    if(STN_UNDEF==idx)
    {
      fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
      return STN_UNDEF;
    }
    GElf_Sym sym;
    
    if(!gelf_getsym(symTabData,idx,&sym))
    {
      fprintf(stderr,"Error getting symbol with idx %i from symbol table. Error is: %s\n",idx,elf_errmsg(-1));
      
    }
    symname=elf_strptr(e,strTblIdx,sym.st_name);
    //update index for the next go-around if need be from the chain table
    memcpy(&nextIdx,hashTableData->d_buf+sizeof(Elf32_Word)*(2+nbucket+idx),sizeof(Elf32_Word));
  }
  return idx;
  */

  Elf_Data* symTabData=NULL;
  char* (*getstrfunc)(ElfInfo*,int)=NULL;
  if(flags & ESFF_NEW_DYNAMIC)
  {
    symTabData=getDataByERS(e,ERS_DYNSYM);
    getstrfunc=&getDynString;
  }
  else
  {
    symTabData=getDataByERS(e,ERS_SYMTAB);
    getstrfunc=&getString;
  }

  char* symbolNameUnmangled=symbolName;
  if(flags & ESFF_MANGLED_OK)
  {
    symbolNameUnmangled=unmangleSymbolName(symbolName);
  }



  bool found=false;
  //traverse the symbol table to find the symbol we're looking for. Yes this is slow
  //todo: build our own hash table since the .hash section seems incomplete
  int i;
  for (i = 0; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    gelf_getsym(symTabData,i,&sym);
    char* symname=(*getstrfunc)(e,sym.st_name);
    char* symnameUnmangled=symname;
    if(flags & ESFF_MANGLED_OK)
    {
      symnameUnmangled=unmangleSymbolName(symname);
    }
    if(!strcmp(symnameUnmangled,symbolNameUnmangled))
    {
      found=true;
    }
    if(symnameUnmangled!=symname)
    {
      free(symnameUnmangled);
    }
    if(found)
    {
      break;
    }
  }
  if(symbolNameUnmangled!=symbolName)
  {
    free(symbolNameUnmangled);
  }
  if(found)
  {
    return i;
  }
  logprintf(ELL_INFO_V1,ELS_SYMBOL,"Symbol '%s' not defined yet. This may or may not be a problem\n",symbolName);
  return STN_UNDEF;
}

//find the index of a symbol whose st_value is addr or where
//addr>st_value && addr<st_value+st_size
//only match symbols whose type is type and are for section scnIdx
//pass SHN_UNDEF for scnIdx to accept symbols referencing any section
//todo: this is slow. Could do smth much faster with interval tress or something
idx_t findSymbolContainingAddress(ElfInfo* e,addr_t addr,byte type,idx_t scnIdx)
{
  Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
  for (int i = 1; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    if(!gelf_getsym(symTabData,i,&sym))
    {death("gelf_getsym failed\n");}
    if(ELF32_ST_TYPE(sym.st_info)==type &&
       (sym.st_shndx==scnIdx || SHN_UNDEF==scnIdx) &&
       (sym.st_value==addr || 
        (sym.st_value<=addr &&
         sym.st_value+sym.st_size > addr)))
       
    {
      return i;
    }
  }
  return STN_UNDEF;
}

