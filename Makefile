PKGCONFIG= pkg-config
PACKAGES= libusb-1.0 librtlsdr

# FLAGS will be passed to both the C and C++ compiler

FLAGS += $(shell $(PKGCONFIG) --cflags $(PACKAGES))
CFLAGS +=
CXXFLAGS +=

# Add .cpp and .c files to the build
SOURCES = $(wildcard src/*.cpp src/*.c src/*/*.cpp src/*/*.c)

# Must include the VCV plugin Makefile framework
include ../../plugin.mk

# Careful about linking to libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine.
ifeq ($(ARCH), lin)
	# WARNING: static compilation is broken on Linux
	LDFLAGS +=$(shell $(PKGCONFIG) --libs $(PACKAGES))
endif

ifeq ($(ARCH), mac)
	LDFLAGS +=$(shell $(PKGCONFIG) --variable=libdir libusb-1.0)/libusb-1.0.a
	LDFLAGS +=$(shell $(PKGCONFIG) --variable=libdir librtlsdr)/librtlsdr.a
endif

ifeq ($(ARCH), win)
	LDFLAGS +=$(shell $(PKGCONFIG) --variable=libdir librtlsdr)/librtlsdr_static.a
	LDFLAGS +=$(shell $(PKGCONFIG) --variable=libdir libusb-1.0)/libusb-1.0.a
endif


# Convenience target for including files in the distributable release
DIST_NAME = PulsumQuadratum-rtlsdr
.PHONY: dist
dist: all
ifndef VERSION
	$(error VERSION must be defined when making distributables)
endif
	mkdir -p dist/$(DIST_NAME)
	cp LICENSE* dist/$(DIST_NAME)/
	cp $(TARGET) dist/$(DIST_NAME)/
	cp -R res dist/$(DIST_NAME)/
	cd dist && zip -5 -r $(DIST_NAME)-$(VERSION)-$(ARCH).zip $(DIST_NAME)
