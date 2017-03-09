VERSION=1.4.4
TIMESTAMP:=$(shell date "+%F %T%z")

CFLAGS?=-Wall -ggdb -O2
LIB_CFLAGS:=${CFLAGS} ${CPPFLAGS} -I. -I.. -I
LIB_CXXFLAGS:=$(LIB_CFLAGS) ${CPPFLAGS}
LIB_LDFLAGS:=${LDFLAGS}

CLIENT_CFLAGS:=${CFLAGS} ${CPPFLAGS} -I LIB -DVERSION="\"${VERSION}\""
CLIENT_LDFLAGS:=$(LDFLAGS) -lrt -lmysqlclient

LIB_CFLAGS:=$(LIB_CFLAGS) -fPIC
LIB_CXXFLAGS:=$(LIB_CXXFLAGS) -fPIC

MAKE_ALL:=infi
INSTALL?=install
prefix=/usr/local
STRIP?=strip

.PHONY: all install uninstall reallyclean clean

all : CloudInfiswift_sync

CloudInfiswift_sync : CloudInfiswift_sync.o 
	${CROSS_COMPILE}${CC} $^ -o $@ ${CLIENT_LDFLAGS}

CloudInfiswift_sync.o : CloudInfiswift_sync.c CloudInfiswift_sync.h
	${CROSS_COMPILE}$(CC) $(LIB_CFLAGS) -c $< -o $@

install : all
	$(INSTALL) -d ${DESTDIR}$(prefix)/bin
	$(INSTALL) -s --strip-program=${CROSS_COMPILE}${STRIP} CloudInfiswift_sync ${DESTDIR}${prefix}/bin/CloudInfiswift_sync

uninstall :
	
	-rm -f ${DESTDIR}${prefix}/bin/CloudInfiswift_sync

reallyclean : clean

clean : 

	-rm -f *.o CloudInfiswift_sync




