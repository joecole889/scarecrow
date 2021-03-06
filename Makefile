################################################################################
# Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
################################################################################

APP:= scarecrow
GSTPLUGIN:= libgstzedsplit.so

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)

NVDS_VERSION:=5.0

LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
GST_INSTALL_DIR?=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0/
APP_INSTALL_DIR?=.

ifeq ($(TARGET_DEVICE),aarch64)
  CFLAGS:= -DPLATFORM_TEGRA
endif

PKGS:= gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-1.0 gstreamer-rtsp-server-1.0

OBJS:= $(SRCS:.c=.o)

OBJS+= Robot.o StateMachine.o

CFLAGS+= -ggdb -fPIC -I/opt/nvidia/deepstream/deepstream-5.0/sources/includes
CFLAGS+= -I/home/joe/sd/code/DynamixelSDK/c++/include/dynamixel_sdk
CFLAGS+= `pkg-config --cflags $(PKGS)`

LIBS:= `pkg-config --libs $(PKGS)`
LIBS+= -L$(LIB_INSTALL_DIR) -L$(LIB_INSTALL_DIR)/gst-plugins/
LIBS+= -lm -lnvdsgst_meta -lnvds_meta -lnvdsgst_dsexample
LIBS+= -L/usr/local/lib/ -ldxl_arch64_cpp
LIBS+= -Wl,-rpath,$(LIB_INSTALL_DIR)

GSTSRCS:= gstzedsplit.c metadepth.c
GSTINCS:= $(GSTSRCS:.c=.h)
GSTOBJS:= $(GSTSRCS:.c=.o)
GSTCFLAGS:= -ggdb -fPIC -I/home/joe/sd/code/gst-template/gst-plugin/src
GSTCFLAGS+= -I/opt/nvidia/deepstream/deepstream-5.0/sources/includes
GSTCFLAGS+= `pkg-config --cflags gstreamer-1.0`
GSTLIBS:= -shared -L$(LIB_INSTALL_DIR)
GSTLIBS+= -lnvdsgst_meta -lnvds_meta

all: $(APP) $(GSTPLUGIN)
.PHONY: all gst

gst: $(GSTPLUGIN)

gstzedsplit.o: gstzedsplit.c $(GSTINCS) Makefile
	$(CC) -c -o $@ $(GSTCFLAGS) $<

metadepth.o: metadepth.c metadepth.h Makefile
	$(CC) -c -o $@ $(GSTCFLAGS) $<

$(GSTPLUGIN): $(GSTOBJS) Makefile
	$(CC) -o $@ $(GSTOBJS) $(GSTLIBS)

%.o: %.cpp $(INCS) Makefile
	$(CXX) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CXX) -o $(APP) $(OBJS) $(LIBS)

install: $(GSTPLUGIN) #$(APP)
	sudo chmod -x $(GSTPLUGIN)
	sudo cp $(GSTPLUGIN) $(GST_INSTALL_DIR)
	#cp -rv $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(OBJS) $(APP) $(GSTPLUGIN)
