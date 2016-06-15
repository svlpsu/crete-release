.PHONY: boost distclean install uninstall distclean

all: boost

boost.src:
	if [ ! -d boost.src ]; then tar -jxvf boost.src.tar.bz2; fi
	cd boost.src && ./bootstrap.sh --includedir="../../include" --libdir="../../bin"
boost: boost.src
	cd boost.src && ./b2 install
install: boost
	cd boost.src && ./bootstrap.sh && ./b2 install
uninstall: 
	cd boost.src && ./b2 uninstall
distclean:
	rm -f ../bin/libboost*
	rm -rf ../include/boost
	rm -rf ./boost.src
