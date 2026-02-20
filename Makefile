#---------------------------------------------------------------------------------
# WiiMedic - Wii System Diagnostic & Health Monitor
# Makefile for devkitPPC / libogc (Flattened for Windows Robustness)
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	boot
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	source

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(dir)) \
			-I$(BUILD) \
			-I"C:/devkitPro/libogc/include"

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
LIBPATHS :=	-L"C:/devkitPro/libogc/lib/wii"

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS		:=	-g -O2 -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS	:=	$(CFLAGS)
LDFLAGS		:=	-g $(MACHDEP) -Wl,-Map,$(TARGET).map

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-lwiiuse -lbte -lfat -logc -lm

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(wildcard $(SOURCES)/*.c)
CPPFILES	:=	$(wildcard $(SOURCES)/*.cpp)
sFILES		:=	$(wildcard $(SOURCES)/*.s)
SFILES		:=	$(wildcard $(SOURCES)/*.S)

OFILES		:=	$(CFILES:$(SOURCES)/%.c=$(BUILD)/%.o) \
				$(CPPFILES:$(SOURCES)/%.cpp=$(BUILD)/%.o) \
				$(sFILES:$(SOURCES)/%.s=$(BUILD)/%.o) \
				$(SFILES:$(SOURCES)/%.S=$(BUILD)/%.o)

VERSION		:=	1.2.0
DIST_DIR	:=	$(CURDIR)/dist

.PHONY: all clean dist install

all: $(BUILD) $(TARGET).dol

$(BUILD):
	@if [ ! -d "$(BUILD)" ]; then mkdir -p "$(BUILD)"; fi

$(TARGET).dol: $(TARGET).elf
	@echo creating $(notdir $@)
	@elf2dol "$<" "$@"

$(TARGET).elf: $(OFILES)
	@echo linking $(notdir $@)
	@$(CC) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o "$@"

$(BUILD)/%.o: $(SOURCES)/%.c
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: $(SOURCES)/%.cpp
	@echo $(notdir $<)
	@$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: $(SOURCES)/%.s
	@echo $(notdir $<)
	@$(CC) -x assembler-with-cpp $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: $(SOURCES)/%.S
	@echo $(notdir $<)
	@$(CC) -x assembler-with-cpp $(CFLAGS) -c "$<" -o "$@"

clean:
	@echo clean ...
	@rm -rf "$(BUILD)" "$(TARGET).elf" "$(TARGET).dol" "$(TARGET).map"

install: all
	@echo Installing to $(INSTALL_DIR) ...
	@mkdir -p "$(INSTALL_DIR)"
	@cp -v "$(TARGET).dol" "$(INSTALL_DIR)/boot.dol"
	@cp -v "$(CURDIR)/meta.xml" "$(INSTALL_DIR)/"
	@cp -v "$(CURDIR)/icon.png" "$(INSTALL_DIR)/"
	@echo Done.

dist: all
	@echo Creating release zip...
	@rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_DIR)/WiiMedic"
	@cp -v "$(TARGET).dol" "$(DIST_DIR)/WiiMedic/boot.dol"
	@cp -v "$(CURDIR)/meta.xml" "$(DIST_DIR)/WiiMedic/"
	@if [ -f "$(CURDIR)/icon.png" ]; then cp -v "$(CURDIR)/icon.png" "$(DIST_DIR)/WiiMedic/"; fi
	@cd "$(DIST_DIR)" && zip -r "$(CURDIR)/WiiMedic_v$(VERSION).zip" WiiMedic
	@rm -rf "$(DIST_DIR)"
	@echo Done. Release: WiiMedic_v$(VERSION).zip
