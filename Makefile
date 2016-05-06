################################   MAKEFILE OPTIONS     ####################################

ENABLE_GRAPHICS = false

COMPILER = g++

OPTIMIZATION_LEVEL = -O3
# can be -O0 (no optimization) to -O3 (full optimization), or -Os (optimize space)

#############################################################################################

EXE = wotan

DEBUG_FLAGS = -g

CC = $(COMPILER)


LIB_DIR = -L.
LIB = -lwotan -lpthread

SRC_DIR = SRC
OBJ_DIR = OBJ

OBJ = $(patsubst $(SRC_DIR)/%.cxx, $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cxx $(SRC_DIR)/*/*.cxx))
DEP := $(OBJ:.o=.d)


OBJ_DIRS = $(sort $(dir $(OBJ)))
INC_DIRS = -ISRC/base -ISRC/parse -ISRC/analysis -ISRC/draw
INC_DIRS := $(INC_DIRS) \
	$(shell pkg-config --cflags freetype2)
 
# Silently create target directories as need
#$(OBJ_DIRS):
#	@ mkdir -p $@


ifneq (,$(findstring true, $(ENABLE_GRAPHICS)))
  # The following block defines the graphics library directories. If X11 library
  # or fontconfig is located elsewhere, change it here.
  ifneq (,$(findstring true, $(MAC_OS)))
    LIB_DIR := $(LIB_DIR) -L/usr/X11/lib -L/opt/X11/lib
  else  
    LIB_DIR := $(LIB_DIR) -L/usr/lib/X11
    PACKAGEINSTALL := if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libx11-dev -c >>/dev/null; then sudo apt-get install libx11-dev; fi; fi;
    PACKAGEINSTALL := if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libxft-dev -c >>/dev/null; then sudo apt-get install libxft-dev; fi; fi;
    PACKAGENOTIFICATION :=                        if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libx11-dev -c >>/dev/null; then printf "\n\n\n\n*****************************************************\n* VPR has detected that graphics are enabled,       *\n* but the required graphics package libx11-dev      *\n* is missing. Try:                                  *\n* a) Type 'make packages' to install libx11-dev     *\n*    automatically if not already installed.        *\n* b) Type 'sudo apt-get install libx11-dev' to      *\n*    install manually.                              *\n* c) If libx11-dev is installed, point the Makefile *\n*    to where your X11 libraries are installed.     *\n* d) If you wish to run VPR without graphics, set   *\n*    the flag ENABLE_GRAPHICS = false at the top    *\n*    of the Makefile in VPR's parent directory.     *\n*****************************************************\n\n\n\n"; fi; fi;
    PACKAGENOTIFICATION := $(PACKAGENOTIFICATION) if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libxft-dev -c >>/dev/null; then printf "\n\n\n\n*****************************************************\n* VPR has detected that graphics are enabled,       *\n* but the required graphics package libxft-dev      *\n* is missing. Try:                                  *\n* a) Type 'make packages' to install libxft-dev     *\n*    automatically if not already installed.        *\n* b) Type 'sudo apt-get install libxft-dev' to      *\n*    install manually.                              *\n* c) If libxft-dev is installed, point the Makefile *\n*    to where your X11 libraries are installed.     *\n* d) If you wish to run VPR without graphics, set   *\n*    the flag ENABLE_GRAPHICS = false at the top    *\n*    of the Makefile in VPR's parent directory.     *\n*****************************************************\n\n\n\n"; fi; fi;
  endif
  LIB := $(LIB) -lX11 -lXft -lfontconfig
else
  FLAGS := $(FLAGS) -DNO_GRAPHICS
endif



WARN_FLAGS = -Wall -Wpointer-arith -Wcast-qual -D__USE_FIXED_PROTOTYPES__ -ansi -pedantic -Wshadow -Wcast-align -D_POSIX_SOURCE -Wno-write-strings
#should -Wcast-qual be -Wcast-equal??

OPT_FLAGS = $(OPTIMIZATION_LEVEL) -std=c++11

FLAGS := $(FLAGS) $(WARN_FLAGS) $(OPT_FLAGS)  -D EZXML_NOMMAP -D_POSIX_C_SOURCE $(DEBUG_FLAGS)

$(EXE): libwotan.a Makefile 
	$(CC) $(FLAGS) OBJ/main.o -o $@ $(LIB_DIR) $(LIB)


#create .a library but remove main.o
libwotan.a: $(OBJ)
	@ ar rcs $@ $(OBJ)
	@ ar d $@ main.o

# Enable a second round of expansion so that we may include
# the target directory as a prerequisite of the object file.
.SECONDEXPANSION:

# The directory follows a "|" to use an existence check instead of the usual
# timestamp check.  Every write to the directory updates the timestamp thus
# without this, all but the last file written to a directory would appear
# to be out of date.
$(OBJ): OBJ/%.o:$(SRC_DIR)/%.cxx | $$(dir $$@D)
	$(CC) $(FLAGS) -MD -MP $(INC_DIRS)  -c $< -o $@

# Silently create target directories as need
$(OBJ_DIRS):
	@ mkdir -p $@

-include $(DEP)


clean:
	rm -f $(EXE) $(OBJ) $(DEP)

#for debugging. use by typing make at command line followed by print-<variable> to display the variable
print-%: ; @echo $* is $($*)
