# $Id: zlib.mak,v 1.3 2003/10/16 18:50:56 jasta Exp $

# If you wish to reduce the memory requirements (default 256K for big
# objects plus a few K), you can add to CFLAGS below: 
#   -DMAX_MEM_LEVEL=7 -DMAX_WBITS=14
# See zconf.h for details about the memory requirements.

# make release by default
!if !defined(debug) && !defined(DEBUG)
RELEASE=1
!endif


CC=cl
LD=link
CFLAGS=-nologo -W3
LDFLAGS=-nologo /subsystem:console binmode.obj oldnames.lib /machine:I386
O=.obj

!if defined(release) || defined(RELEASE)
D=
CFLAGS  = $(CFLAGS)  -Ox -GA3s
DEFS    = $(DEFS)    -DNDEBUG -U_DEBUG
LDFLAGS = $(LDFLAGS) -release
!else
D=d
CFLAGS  = $(CFLAGS)  -Od
DEFS    = $(DEFS)    -D_DEBUG -UNDEBUG -D_WALL
LDFLAGS = $(LDFLAGS) -debug
!endif

!if defined(static) || defined(STATIC)
CFLAGS  = $(CFLAGS) -MT$(D)
!else
CFLAGS  = $(CFLAGS) -MD$(D)
!endif

OBJECTS  =  \
	adler32$(O) \
	compress$(O) \
	crc32$(O) \
	deflate$(O) \
	gzio$(O) \
	infblock$(O) \
	infcodes$(O) \
	inffast$(O) \
	inflate$(O) \
	inftrees$(O) \
	infutil$(O) \
	trees$(O) \
	uncompr$(O) \
	zutil$(O)

ZLIB_DLL = zlib$(D).dll
ZLIB_LIB = zlib$(D).lib
ZLIB_LIB_STATIC = zlibstatic$(D).lib

!if defined(static) || defined(STATIC)
all:  $(ZLIB_LIB_STATIC) example.exe minigzip.exe
!else
all:  $(ZLIB_DLL) example.exe minigzip.exe
!endif

adler32.obj: adler32.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

compress.obj: compress.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

crc32.obj: crc32.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

deflate.obj: deflate.c deflate.h zutil.h zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

gzio.obj: gzio.c zutil.h zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

infblock.obj: infblock.c zutil.h zlib.h zconf.h infblock.h inftrees.h infcodes.h infutil.h
	$(CC) -c $(CFLAGS) $*.c

infcodes.obj: infcodes.c zutil.h zlib.h zconf.h inftrees.h infutil.h infcodes.h inffast.h
	$(CC) -c $(CFLAGS) $*.c

inflate.obj: inflate.c zutil.h zlib.h zconf.h infblock.h
	$(CC) -c $(CFLAGS) $*.c

inftrees.obj: inftrees.c zutil.h zlib.h zconf.h inftrees.h
	$(CC) -c $(CFLAGS) $*.c

infutil.obj: infutil.c zutil.h zlib.h zconf.h inftrees.h infutil.h
	$(CC) -c $(CFLAGS) $*.c

inffast.obj: inffast.c zutil.h zlib.h zconf.h inftrees.h infutil.h inffast.h
	$(CC) -c $(CFLAGS) $*.c

trees.obj: trees.c deflate.h zutil.h zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

uncompr.obj: uncompr.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

zutil.obj: zutil.c zutil.h zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

example.obj: example.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

minigzip.obj: minigzip.c zlib.h zconf.h
	$(CC) -c $(CFLAGS) $*.c

$(ZLIB_LIB_STATIC): $(OBJECTS)
	@if exist $(ZLIB_LIB_STATIC) del $(ZLIB_LIB_STATIC)
	lib /OUT:$(ZLIB_LIB_STATIC) $(OBJECTS)

$(ZLIB_DLL): $(OBJECTS) nt\zlib.dnt
	@if exist $(ZLIB_LIB) del $(ZLIB_LIB)
	@if exist $(ZLIB_DLL) del $(ZLIB_DLL)
	$(LD) $(dlllflags) -dll -out:$(ZLIB_DLL) -implib:$(ZLIB_LIB) -def:nt\zlib.dnt $(OBJECTS)
  
example.exe: example.obj $(ZLIB_LIB) 
	$(LD) $(LDFLAGS) example.obj $(ZLIB_LIB) /OUT:example.exe

minigzip.exe: minigzip.obj $(ZLIB_LIB) 
	$(LD) $(LDFLAGS) minigzip.obj $(ZLIB_LIB) /OUT:minigzip.exe

test: example.exe minigzip.exe
	@example
	@echo hello world | minigzip | minigzip -d 

clean:
	-del *.obj
	-del *.lib
	-del *.dll
	-del *.exe
	-del *.ilk
	-del *.pdb
	-del *.exp
