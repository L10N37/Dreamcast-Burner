# OBS! M4 processed!
changequote([, ])
[
CHAIN=growisofs dvd+rw-format dvd+rw-booktype dvd+rw-mediainfo dvd-ram-control

dvd+rw-tools:	$(CHAIN)

WARN=#-Wall	# developers are welcomed to build with `make WARN=-Wall'
]

CC	=gcc
CFLAGS	+=$(WARN) -O2
CXX	=g++
CXXFLAGS+=$(WARN) -O2 -fno-exceptions
LINK.o	=$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
LDFLAGS+=-lstdc++

# for 32-bit / x86 ??? I honestly don't remember what I did
# CFLAGS	+=-m32
# CXXFLAGS+=-m32

# common section
[
growisofs:		growisofs_mmc.o growisofs.o
growisofs.o:		growisofs.c mp.h
growisofs_mmc.o:	growisofs_mmc.cpp transport.hxx asctable.h
asctable.h:		keys.txt
	perl genasctable.pl < keys.txt > $@

dvd+rw-format:		dvd+rw-format.o
dvd+rw-format.o:	dvd+rw-format.cpp transport.hxx asctable.h

dvd+rw-mediainfo:	dvd+rw-mediainfo.o
dvd+rw-mediainfo.o:	dvd+rw-mediainfo.cpp transport.hxx asctable.h

dvd+rw-booktype:	dvd+rw-booktype.o
dvd+rw-booktype.o:	dvd+rw-booktype.cpp transport.hxx asctable.h

dvd-ram-control:	dvd-ram-control.o
dvd-ram-control.o:	dvd-ram-control.cpp transport.hxx asctable.h

rpl8:			rpl8.o
rpl8.o:			rpl8.cpp transport.hxx asctable.h
+rpl8:			rpl8
#so that I can invoke `make +rpl8' to build rpl8...
btcflash:		btcflash.o
btcflash.o:		btcflash.cpp transport.hxx asctable.h
+btcflash:		btcflash
#so that I can invoke `make +btcflash' to build btcflash...
panaflash:		panaflash.o
panaflash.o:		panaflash.cpp transport.hxx asctable.h
+panaflash:		panaflash
]
