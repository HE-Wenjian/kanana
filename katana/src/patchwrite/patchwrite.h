/*
  File: patchwrite.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Write patch information out to a po (patch object) file
*/

#ifndef patchwrite_h
#define patchwrite_h
void writePatch(char* oldSourceTree,char* newSourceTree,char* oldBinaryName,char* newBinaryName,char* patchOutName);
#endif
