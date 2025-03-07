# The MIT License (MIT)

# Copyright (c) 2017 nabijaczleweli

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


include configMakefile


LDAR := $(PIC) $(foreach l,zstd whereami-cpp inih,-L$(BLDDIR)$(l)) $(foreach dll,zstd whereami++ inih,-l$(dll))
INCAR := $(foreach l,$(foreach l,whereami-cpp json,$(l)/include) totalcmd-wcx-api inih,-isystemext/$(l)) $(foreach l,zstd,-isystem$(BLDDIR)$(l)/include)
VERAR := $(foreach l,TOTALCMD_ZSTD WHEREAMI_CPP JSON INIH,-D$(l)_VERSION='$($(l)_VERSION)')
SOURCES := $(sort $(wildcard src/*.cpp src/**/*.cpp src/**/**/*.cpp src/**/**/**/*.cpp))

.PHONY : all clean zstd whereami-cpp inih wcx

all : zstd whereami-cpp inih wcx

clean :
	rm -rf $(OUTDIR)

wcx : $(OUTDIR)totalcmd-zstd$(WCX)
zstd : $(BLDDIR)zstd/libzstd$(ARCH) $(BLDDIR)zstd/include/zstd/zstd.h
whereami-cpp : $(BLDDIR)whereami-cpp/libwhereami++$(ARCH)
inih : $(BLDDIR)inih/libinih$(ARCH)


$(OUTDIR)totalcmd-zstd$(WCX) : $(subst $(SRCDIR),$(OBJDIR),$(subst .cpp,$(OBJ),$(SOURCES)))
	$(CXX) $(CXXAR) -shared -o$@ $^ $(PIC) $(LDAR)

$(BLDDIR)zstd/libzstd$(ARCH) : $(subst ext/zstd/lib,$(BLDDIR)zstd/obj,$(subst .c,$(OBJ),$(subst .S,$(OBJ),$(foreach subdir,common compress decompress,$(wildcard ext/zstd/lib/$(subdir)/*.c ext/zstd/lib/$(subdir)/*.S)))))
	@mkdir -p $(dir $@)
	$(AR) --thin crs $@ $^

$(BLDDIR)zstd/include/zstd/zstd.h : $(wildcard ext/zstd/lib/*.h ext/zstd/lib/common/*.h ext/zstd/lib/compress/*.h ext/zstd/lib/decompress/*.h)
	@mkdir -p $(foreach incfile,$(subst ext/zstd/lib,$(BLDDIR)zstd/include/zstd,$^),$(abspath $(dir $(incfile))))
	$(foreach incfile,$^,cp $(incfile) $(subst ext/zstd/lib,$(BLDDIR)zstd/include/zstd,$(incfile));)

$(BLDDIR)whereami-cpp/libwhereami++$(ARCH) : ext/whereami-cpp/Makefile
	$(MAKE) -C$(dir $^) BUILD=$(abspath $(dir $@)) stlib

$(BLDDIR)inih/libinih$(ARCH) : $(BLDDIR)inih/obj/ini$(OBJ)
	@mkdir -p $(dir $@)
	$(AR) --thin crs $@ $^


$(OBJDIR)%$(OBJ) : $(SRCDIR)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXAR) $(INCAR) $(VERAR) -DZSTD_STATIC_LINKING_ONLY -c -o$@ $^

$(BLDDIR)zstd/obj/%$(OBJ) : ext/zstd/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CCAR) -Iext/zstd/lib -Iext/zstd/lib/common -c -o$@ $^

$(BLDDIR)zstd/obj/%$(OBJ) : ext/zstd/lib/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CCAR) -Iext/zstd/lib -Iext/zstd/lib/common -c -o$@ $^

$(BLDDIR)inih/obj/%$(OBJ) : ext/inih/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CCAR) -DINI_USE_STACK=0 -DINI_MAX_LINE=2048 -c -o$@ $^
