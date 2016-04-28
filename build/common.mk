#
# 自动编译脚本（2011-09）
# *支持多目录共同编译
# *支持头文件depends
#

# makefile name
MFNAME = $(word 1, $(MAKEFILE_LIST))

# debug option
ifeq ($(DEBUG), 1)
NAME = debug_$(MFNAME)
CFLAGS += -O0 -g
else
NAME = release_$(MFNAME)
CFLAGS += -O2 -DNDEBUG
endif

# tool chain
CPP	= $(CROSS)g++
CC	= $(CROSS)gcc
LD	= $(CROSS)ld
AR  = $(CROSS)ar
RM	= rm -f
STRIP	= @echo " strip  $@"; $(CROSS)strip

# make src dirs
SRCDIR := $(foreach dir, $(SRCSUBDIR), $(SRCPATH)/$(dir)) $(SRCPATH)

# src files
SRCS_C := $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
SRCS_CPP := $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.cpp))

# obj dirs
OBJPATH := .obj_$(NAME)
OBJSUBDIR := $(SRCSUBDIR)

# output binary dir
BINDIR := $(OBJPATH)/.out

# obj files
OBJS_C := $(patsubst %.c, %.o, ${SRCS_C})
OBJS_CPP := $(patsubst %.cpp, %.o, ${SRCS_CPP})
OBJS_ORIG := $(sort $(OBJS_C) $(OBJS_CPP))
OBJS := $(patsubst $(SRCPATH)/%, $(OBJPATH)/%, ${OBJS_ORIG})

# depends
OBJD_CPP := $(patsubst %.o, %.cpp.d, $(OBJS_CPP))
OBJD_C := $(patsubst %.o, %.c.d, $(OBJS_C))
OBJD_ORIG := $(OBJD_C) $(OBJD_CPP)
OBJD := $(patsubst $(SRCPATH)/%, $(OBJPATH)/%, ${OBJD_ORIG})

# handle inlcude -I options
INCS := $(foreach inc, $(INCDIR), -I$(inc))

# handle lib options
LDLIBS := $(foreach lp, $(LIBDIR), -L$(lp)) -L$(shell pwd) $(foreach ln, $(LIBS), -l$(ln)) 

# prepare obj tmp dir
ifneq ($(strip ${MAKECMDGOALS}), clean)
$(shell mkdir -p $(OBJPATH))
$(foreach dir, $(OBJSUBDIR), $(shell mkdir -p $(OBJPATH)/$(dir)))
$(shell mkdir -p $(BINDIR))
endif

# entry
all: start depends $(TARGET) install end

start:
	@$(START_CMD1)
	@$(START_CMD2)
	@echo [[[ START $(TARGET) $(NAME) ]]]

end:
	@echo [[[ THE END ]]]

# build
$(TARGET): $(OBJS) $(USER_LIBS)
	@echo [[[ OUTPUT ]]]
# 1 - static lib
ifeq ($(BINARY), static)
	@echo " ar $(TARGET)"
	@$(AR) -r $(BINDIR)/$@ $^
# 2 - shared lib
else ifeq ($(BINARY), shared)
	@echo " make $(TARGET)"
	@$(CPP) $^ -shared $(LDLIBS) $(LDFLAGS) -o $(BINDIR)/$@
# 3 - exec
else ifeq ($(BINARY), exec)
	@echo " make $(TARGET)"
	@$(CPP) $^ $(LDLIBS) $(LDFLAGS) -o $(BINDIR)/$@
# 4 - else
else
	@echo NOT support build type, exit.
	@exit
endif
	
# compile	
$(OBJPATH)/%.o: $(SRCPATH)/%.c
	@echo " $(CC) $(patsubst $(SRCPATH)/%, %, $<)"
	@$(CC) -c $(INCS) $(CFLAGS) $< -o $@
$(OBJPATH)/%.o: $(SRCPATH)/%.cpp
	@echo " $(CPP) $(patsubst $(SRCPATH)/%, %, $<)"
	@$(CPP) -c $(INCS) $(CFLAGS) $< -o $@

# depends
depends: $(OBJD)
	@echo " make depends"

$(OBJPATH)/%.c.d: $(SRCPATH)/%.c
	@$(CC) $(CFLAGS) $(INCS) -MM -E $^ > $@
	@sed 's/.*\.o/$(subst /,\/, $(dir $@))&/g' $@ >$@.tmp
	@mv $@.tmp $@
$(OBJPATH)/%.cpp.d: $(SRCPATH)/%.cpp
	@$(CPP) $(CFLAGS) $(INCS) -MM -E $^ > $@
	@sed 's/.*\.o/$(subst /,\/, $(dir $@))&/g' $@ >$@.tmp
	@mv $@.tmp $@
	
# include depends
ifneq ($(strip ${MAKECMDGOALS}), clean)
-include $(OBJD)
endif

# install
install:
	@echo " cp to $(INSTALL_PATH)/$(TARGET)"
	@cp $(BINDIR)/$(TARGET) $(INSTALL_PATH)/
	
# clean
clean:
	@echo [[[ CLEAN $(TARGET) $(NAME) ]]] 
	rm -rf $(OBJPATH)
	rm -f $(INSTALL_PATH)/$(TARGET)

