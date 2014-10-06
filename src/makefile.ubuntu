# Copyright (c) 2009-2010 Satoshi Nakamoto
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#######################################################################
#
# INSTRUCTIONS TO BUILD ON UBUNTU (EASY!)
#
# These instructions use standard ubuntu packages and assume a
# fresh Ubuntu 14.0.4 install.
#
# First, install git and clone the repo:
#
#    sudo apt-get install git
#    git clone https://github.com/StealthSend/Stealth.git
#
# Make sure you have also installed the
# following packages via apt-get
#
#     sudo apt-get install build-essential
#     sudo apt-get install libssl-dev
#     sudo apt-get install zlib1g-dev
#     sudo apt-get install libboost-all-dev
#     sudo apt-get install libevent-dev
#     sudo apt-get install libcrypto++-dev
#     sudo apt-get install libdb++-dev
#
# These commands will make sure you have the correct headers and will
# put the correct static libraries into the correct places
#
# Then, you can build
#
#     make -f makefile.ubuntu
#
#######################################################################


# Build STATIC for this makefile (1=static, 0=not)
STATIC=1

# Berkely DB
DEPSDIR=/usr/local

# debian third-party headers
INC_LINUX="/usr/include/x86_64-linux-gnu"
# debian third-party libs
LIB_LINUX="/usr/lib/x86_64-linux-gnu"


INCLUDEPATHS= \
 -I"$(CURDIR)" \
 -I"$(CURDIR)/obj" \
 -I"$(CURDIR)/tor" \
 -I"$(CURDIR)/tor/adapter" \
 -I"$(CURDIR)/tor/common" \
 -I"$(CURDIR)/tor/ext" \
 -I"$(CURDIR)/tor/ext/curve25519_donna" \
 -I"$(CURDIR)/tor/or" \
 -I"/usr/include" \
 -I"$(INC_LINUX)" \
 -I"$(CURDIR)/leveldb/include" \
 -I"$(CURDIR)/leveldb/helpers"

LIBPATHS= \
 -L"/usr/lib" \
 -L"$(LIB_LINUX)" \

CFLAGS += $(INCLUDEPATHS)

USE_UPNP:=-
USE_IPV6:=1

LINK:=$(CXX)

DEFS=-DBOOST_SPIRIT_THREADSAFE

TESTDEFS = -DTEST_DATA_DIR=$(abspath test/data)

ifeq ($(STATIC),1)
TESTLIBS += \
 $(DEPSDIR)/lib/libboost_unit_test_framework.a
LIBS += \
 $(LIB_LINUX)/libdb_cxx.a \
 $(LIB_LINUX)/libboost_system.a \
 $(LIB_LINUX)/libboost_filesystem.a \
 $(LIB_LINUX)/libboost_program_options.a \
 $(LIB_LINUX)/libboost_thread.a \
 $(LIB_LINUX)/libssl.a \
 $(LIB_LINUX)/libcrypto.a \
 $(LIB_LINUX)/libz.a \
 $(LIB_LINUX)/libevent.a \
 /usr/lib/libcryptopp.a \
 $(CURDIR)/leveldb/libleveldb.a \
 $(CURDIR)/leveldb/libmemenv.a
else
TESTLIBS += \
 -lboost_unit_test_framework
endif


LMODE = dynamic
LMODE2 = dynamic
ifdef STATIC
	LMODE = static
	ifeq (${STATIC}, all)
		LMODE2 = static
	endif
else
	TESTDEFS += -DBOOST_TEST_DYN_LINK
endif


LIBS += \
 -Wl,-B$(LMODE) \
   -lboost_system$(BOOST_LIB_SUFFIX) \
   -lboost_filesystem$(BOOST_LIB_SUFFIX) \
   -lboost_program_options$(BOOST_LIB_SUFFIX) \
   -lboost_thread$(BOOST_LIB_SUFFIX) \
   -ldb_cxx$(BDB_LIB_SUFFIX) \
   -lssl \
   -lcrypto \
   -levent \
   -lz \
   -lcryptopp


ifneq (${USE_IPV6}, -)
	DEFS += -DUSE_IPV6=$(USE_IPV6)
endif

LIBS+= \
 -Wl,-B$(LMODE2) \
   -ldl \
   -lpthread


# Hardening
# Make some classes of vulnerabilities unexploitable in case one is discovered.
#
    # This is a workaround for Ubuntu bug #691722, the default -fstack-protector causes
    # -fstack-protector-all to be ignored unless -fno-stack-protector is used first.
    # see: https://bugs.launchpad.net/ubuntu/+source/gcc-4.5/+bug/691722
    HARDENING=-fno-stack-protector

    # Stack Canaries
    # Put numbers at the beginning of each stack frame and check that they are the same.
    # If a stack buffer if overflowed, it writes over the canary number and then on return
    # when that number is checked, it won't be the same and the program will exit with
    # a "Stack smashing detected" error instead of being exploited.
    HARDENING+=-fstack-protector-all -Wstack-protector

    # Make some important things such as the global offset table read only as soon as
    # the dynamic linker is finished building it. This will prevent overwriting of addresses
    # which would later be jumped to.
    LDHARDENING+=-Wl,-z,relro -Wl,-z,now

    # Build position independent code to take advantage of Address Space Layout Randomization
    # offered by some kernels.
    # see doc/build-unix.txt for more information.
    ifdef PIE
        HARDENING+=-fPIE
        LDHARDENING+=-pie
    endif

    # -D_FORTIFY_SOURCE=2 does some checking for potentially exploitable code patterns in
    # the source such overflowing a statically defined buffer.
    HARDENING+=-D_FORTIFY_SOURCE=2
#


DEBUGFLAGS=-g

# CXXFLAGS can be specified on the make command line, so we use xCXXFLAGS that only
# adds some defaults in front. Unfortunately, CXXFLAGS=... $(CXXFLAGS) does not work.
xCXXFLAGS=-O2 -msse2 -pthread -Wall -Wextra -Wformat -Wformat-security -Wno-unused-parameter \
    $(DEBUGFLAGS) $(DEFS) $(HARDENING) $(CXXFLAGS)

# LDFLAGS can be specified on the make command line, so we use xLDFLAGS that only
# adds some defaults in front. Unfortunately, LDFLAGS=... $(LDFLAGS) does not work.
xLDFLAGS=$(LDHARDENING) $(LDFLAGS)

OBJS= \
    obj/address.o \
    obj/addressmap.o \
    obj/aes.o \
    obj/backtrace.o \
    obj/buffers.o \
    obj/channel.o \
    obj/channeltls.o \
    obj/circpathbias.o \
    obj/circuitbuild.o \
    obj/circuitlist.o \
    obj/circuitmux.o \
    obj/circuitmux_ewma.o \
    obj/circuitstats.o \
    obj/circuituse.o \
    obj/command.o \
    obj/tor_compat.o \
    obj/compat_libevent.o \
    obj/config.o \
    obj/config_codedigest.o \
    obj/confparse.o \
    obj/connection.o \
    obj/connection_edge.o \
    obj/connection_or.o \
    obj/container.o \
    obj/control.o \
    obj/cpuworker.o \
    obj/crypto.o \
    obj/csiphash.o \
    obj/crypto_curve25519.o \
    obj/crypto_format.o \
    obj/curve25519-donna.o \
    obj/di_ops.o \
    obj/directory.o \
    obj/dirserv.o \
    obj/dirvote.o \
    obj/dns.o \
    obj/dnsserv.o \
    obj/entrynodes.o \
    obj/ext_orport.o \
    obj/fp_pair.o \
    obj/geoip.o \
    obj/hibernate.o \
    obj/log.o \
    obj/memarea.o \
    obj/mempool.o \
    obj/microdesc.o \
    obj/networkstatus.o \
    obj/nodelist.o \
    obj/onion.o \
    obj/onion_fast.o \
    obj/onion_main.o \
    obj/onion_ntor.o \
    obj/onion_tap.o \
    obj/policies.o \
    obj/procmon.o \
    obj/reasons.o \
    obj/relay.o \
    obj/rendclient.o \
    obj/rendcommon.o \
    obj/rendmid.o \
    obj/rendservice.o \
    obj/rephist.o \
    obj/replaycache.o \
    obj/router.o \
    obj/routerlist.o \
    obj/routerparse.o \
    obj/routerset.o \
    obj/sandbox.o \
    obj/statefile.o \
    obj/status.o \
    obj/tor_util.o \
    obj/torgzip.o \
    obj/tortls.o \
    obj/tor_main.o \
    obj/transports.o \
    obj/util_codedigest.o \
    obj/util_process.o \
    obj/aes_helper.o \
    obj/fugue.o \
    obj/hamsi.o \
    obj/groestl.o \
    obj/blake.o \
    obj/bmw.o \
    obj/skein.o \
    obj/keccak.o \
    obj/shavite.o \
    obj/jh.o \
    obj/luffa.o \
    obj/cubehash.o \
    obj/echo.o \
    obj/simd.o \
    obj/alert.o \
    obj/version.o \
    obj/checkpoints.o \
    obj/netbase.o \
    obj/addrman.o \
    obj/crypter.o \
    obj/key.o \
    obj/db.o \
    obj/init.o \
    obj/irc.o \
    obj/keystore.o \
    obj/main.o \
    obj/net.o \
    obj/protocol.o \
    obj/bitcoinrpc.o \
    obj/rpcdump.o \
    obj/rpcnet.o \
    obj/rpcmining.o \
    obj/rpcwallet.o \
    obj/rpcblockchain.o \
    obj/rpcrawtransaction.o \
    obj/script.o \
    obj/sync.o \
    obj/util.o \
    obj/wallet.o \
    obj/walletdb.o \
    obj/noui.o \
    obj/pbkdf2.o \
    obj/kernel.o \
    obj/scrypt_mine.o \
    obj/scrypt-x86.o \
    obj/scrypt-x86_64.o \
    obj/stealthtext.o \
    obj/stealthaddress.o \
    obj/txdb-leveldb.o 

all: StealthCoind

#
# LevelDB support
#
MAKEOVERRIDES =
LIBS += $(CURDIR)/leveldb/libleveldb.a $(CURDIR)/leveldb/libmemenv.a
DEFS += $(addprefix -I,$(CURDIR)/leveldb/include)
DEFS += $(addprefix -I,$(CURDIR)/leveldb/helpers)
leveldb/libleveldb.a:
	@echo "Building LevelDB ..." && cd leveldb && $(MAKE) CC=$(CC) CXX=$(CXX) OPT="$(xCXXFLAGS)" libleveldb.a libmemenv.a && cd ..


test check: test_stealthcoin FORCE
	./test_stealthcoin

# auto-generated dependencies:
-include obj/*.P
-include obj-test/*.P

obj/build.h: FORCE
	/bin/sh ../share/genbuild.sh obj/build.h
version.cpp: obj/build.h
DEFS += -DHAVE_BUILD_INFO


obj/%.o: %.cpp
	$(CXX) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)


obj/%.o: %.c
	$(CC) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/common/%.c
	$(CC) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/%.c
	$(CC) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/curve25519_donna/%.c
	$(CC) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)


obj/%.o: tor/or/%.c
	$(CC) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)

obj/stealth.o: tor/adapter/stealth.cpp
	$(CXX) -c $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)



obj/scrypt-x86.o: scrypt-x86.S
	$(CXX) -c $(xCXXFLAGS) -MMD -o $@ $<

obj/scrypt-x86_64.o: scrypt-x86_64.S
	$(CXX) -c $(xCXXFLAGS) -MMD -o $@ $<

StealthCoind: leveldb/libleveldb.a \
              $(OBJS:obj/%=obj/%) obj/stealth.o
	$(CXX) $(CFLAGS) -o $@ $(LIBPATHS) $^ $(LIBS)

TESTOBJS := $(patsubst test/%.cpp,obj-test/%.o,$(wildcard test/*.cpp))

obj-test/%.o: test/%.cpp
	$(CXX) -c $(TESTDEFS) $(CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

test_stealthcoin: $(TESTOBJS) $(filter-out obj/init.o,$(OBJS:obj/%=obj/%))
	$(CXX) $(CFLAGS) -o $@ $(LIBPATHS) $^ $(LIBS) $(TESTLIBS)


clean:
	-rm -f StealthCoind test_stealthcoin
	-rm -f obj/*.o
	-rm -f obj-test/*.o
	-rm -f obj/*.P
	-rm -f obj-test/*.P
	-rm -f obj/build.h
	cd leveldb; make clean

FORCE:
