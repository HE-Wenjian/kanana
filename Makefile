CXX=g++
CC=gcc
#todo: remove 32-bit dependency
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE -m32 -I src/
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-L /usr/local/lib -ldwarf -lelf -lm  -lunwind-x86 -lunwind-ptrace

PATCHER_SRC=src/patcher/hotpatch.c src/patcher/target.c src/patcher/patchapply.c src/patcher/versioning.c src/patcher/linkmap.c src/patcher/safety.c
PATCHWRITE_SRC=src/patchwrite/patchwrite.c src/patchwrite/codediff.c src/patchwrite/typediff.c src/patchwrite/elfwriter.c src/patchwrite/sourcetree.c src/patchwrite/write_to_dwarf.c
UTIL_SRC=src/util/dictionary.c src/util/hash.c src/util/util.c src/util/map.c src/util/list.c src/util/logging.c src/util/path.c src/util/refcounted.c
INFO_SRC=src/info/fdedump.c src/info/dwinfo_dump.c
TYPEPATCH_SRC=src/katana.c src/dwarftypes.c   src/elfparse.c  src/types.c  src/dwarf_instr.c src/register.c src/relocation.c src/symbol.c src/fderead.c src/dwarfvm.c src/config.c $(PATCHWRITE_SRC) $(PATCHER_SRC) $(UTIL_SRC) $(INFO_SRC)

EXEC=katana


all :  debug

debug : CFLAGS+=-DDEBUG
debug :  $(EXEC)

release : $(EXEC)


tests :
	make -C tests

code_tests :
	make -C code_tests

.PHONY : clean tests

check : all tests code_tests
	code_tests/listsort
	./run_tests.py

$(EXEC) : $(TYPEPATCH_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_TYPEPATCH)  -o $(EXEC) $(TYPEPATCH_SRC) $(LDFLAGS_TYPEPATCH)



clean :
	rm -f $(EXEC)
	rm -f *.o *.s *.i
	make -C tests clean

#don't you love sed? It does so many marvelous things, but looking at
#this line you haven't the foggiest clue what it might do.
#What it does do is extract the last component of the path name
#so that things in the tar archive can be named appropriately
DIR_TO_TAR=$(shell pwd | sed 's/\(.*\/\)\(.*\)/\2/')
dist : all
	cd ../; tar -czf katana.tar.gz --exclude-from=$(PWD)/tar-excludes $(DIR_TO_TAR)