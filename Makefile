IMAGE                   := beaveros.iso
SRC                     := src
OBJ                     := obj
ISO                     := iso

CFLAGS                  := -std=gnu99 -Wall -Wextra -Wshadow -ffreestanding
LDFLAGS                 := -ffreestanding -nostdlib -z max-page-size=0x1000
LIBS                    := gcc

DIST                    := beaveros

all: $(IMAGE)

TARGETS                 := loader kernel

SRCS_loader             := $(shell find -L $(SRC)/loader -type f | egrep '\.[csS]$$')
CC_loader               := i686-elf-gcc
CFLAGS_loader           = $(CFLAGS) -I$(SRC)/loader/include/
AS_loader               := i686-elf-as
ASFLAGS_loader          = $(ASFLAGS)
AS_CPP_loader           := $(CC_loader) -E
AS_CPPFLAGS_loader      = $(AS_CPPFLAGS)
LD_loader               := $(CC_loader)
LDFLAGS_loader          = $(LDFLAGS) -T $(SRC)/loader/linker.ld
loader.bin: $(SRC)/loader/linker.ld

SRCS_kernel             := $(shell find -L $(SRC) -not -path '$(SRC)/loader/*' -type f | egrep '\.[sc]$$')

CC_kernel               := x86_64-elf-gcc
CFLAGS_kernel           = $(CFLAGS) -I$(SRC)/include/ -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2
AS_kernel               := x86_64-elf-as
ASFLAGS_kernel          = $(ASFLAGS)
AS_CPP_kernel           := $(CC_loader) -E
AS_CPPFLAGS_kernel      = $(AS_CPPFLAGS)
LD_kernel               := $(CC_kernel)
LDFLAGS_kernel          = $(LDFLAGS) -T $(SRC)/linker.ld
kernel.bin: $(SRC)/linker.ld

FLAGS                   := DEBUG
DEBUG                   ?= 0

WITH_DEBUG_CFLAGS       := -O0 -g -DDEBUG
WITH_DEBUG_ASFLAGS      := --gen-debug
WITH_DEBUG_AS_CPPFLAGS  := -DDEBUG
WITH_DEBUG_LDFLAGS      := -O0 -g -DDEBUG

WITHOUT_DEBUG_CFLAGS    := -O2
WITHOUT_DEBUG_LDFLAGS   := -O2

# Do not change below this line, unless you know what are you doing

ENABLED_FLAGS           := $(foreach f,$(FLAGS),$(shell test '$($(f))' == 1 && echo $(f)))
DISABLED_FLAGS          := $(foreach f,$(FLAGS),$(shell test '$($(f))' != 1 && echo $(f)))

CFLAGS                  += $(foreach f,$(ENABLED_FLAGS),$(WITH_$(f)_CFLAGS))
CFLAGS                  += $(foreach f,$(DISABLED_FLAGS),$(WITHOUT_$(f)_CFLAGS))

ASFLAGS                 += $(foreach f,$(ENABLED_FLAGS),$(WITH_$(f)_ASFLAGS))
ASFLAGS                 += $(foreach f,$(DISABLED_FLAGS),$(WITHOUT_$(f)_ASFLAGS))

AS_CPPFLAGS             += $(foreach f,$(ENABLED_FLAGS),$(WITH_$(f)_AS_CPPFLAGS))
AS_CPPFLAGS             += $(foreach f,$(DISABLED_FLAGS),$(WITHOUT_$(f)_AS_CPPFLAGS))

LDFLAGS                 += $(foreach f,$(ENABLED_FLAGS),$(WITH_$(f)_LDFLAGS))
LDFLAGS                 += $(foreach f,$(DISABLED_FLAGS),$(WITHOUT_$(f)_LDFLAGS))
LDFLAGS                 += $(LIBS:%=-l%)

DIRS                    := $(ISO) $(ISO)/boot $(ISO)/boot/grub
QUIET_DIRS              :=
DEPS                    :=

define C_FILE_TEMPLATE
DIRS                    += $(dir $(2:$(SRC)/%=$(OBJ)/%))
QUIET_DIRS              += $(dir $(2:$(SRC)/%=.bmake/%))
DEPS                    += $(2:$(SRC)/%=.bmake/%.d)

$(2:$(SRC)/%=$(OBJ)/%.o): $(2) | $(dir $(2:$(SRC)/%=$(OBJ)/%)) $(dir $(2:$(SRC)/%=.bmake/%))
ifdef VERBOSE
	$$(CC_$(1)) $$(CFLAGS_$(1)) -c $$< -o $$@
else
	@echo 'CC      $$(@:$(OBJ)/%=%)'
	@$$(CC_$(1)) $$(CFLAGS_$(1)) -c $$< -o $$@
endif
	@echo $$(CC_$(1)) $$(CFLAGS_$(1)) >$(2:$(SRC)/%=.bmake/%.b)
	@$$(CC_$(1)) $$(CFLAGS_$(1)) -M $$< | sed -E 's:$(patsubst %.c,%.o,$(notdir $(2))):$(2:$(SRC)/%=$(OBJ)/%.o):' >$(2:$(SRC)/%=.bmake/%.d)

.PHONY: $$(shell echo $$(CC_$(1)) $$(CFLAGS_$(1)) | diff - $(2:$(SRC)/%=.bmake/%.b) 2>/dev/null || echo $(2:$(SRC)/%=$(OBJ)/%.o))

endef

define AS_FILE_TEMPLATE
DIRS                    += $(dir $(2:$(SRC)/%=$(OBJ)/%))
QUIET_DIRS              += $(dir $(2:$(SRC)/%=.bmake/%))

$(2:$(SRC)/%=$(OBJ)/%.o): $(2) | $(dir $(2:$(SRC)/%=$(OBJ)/%)) $(dir $(2:$(SRC)/%=.bmake/%))
ifdef VERBOSE
	$$(AS_$(1)) $$(ASFLAGS_$(1)) -c $$< -o $$@
else
	@echo 'AS      $$(@:$(OBJ)/%=%)'
	@$$(AS_$(1)) $$(ASFLAGS_$(1)) -c $$< -o $$@
endif
	@echo $$(AS_$(1)) $$(ASFLAGS_$(1)) >$(2:$(SRC)/%=.bmake/%.b)

.PHONY: $$(shell echo $$(AS_$(1)) $$(ASFLAGS_$(1)) | diff - $(2:$(SRC)/%=.bmake/%.b) 2>/dev/null || echo $(2:$(SRC)/%=$(OBJ)/%.o))

endef

define AS_CPP_FILE_TEMPLATE
DIRS                    += $(dir $(2:$(SRC)/%=$(OBJ)/%))
QUIET_DIRS              += $(dir $(2:$(SRC)/%=.bmake/%))

$(2:$(SRC)/%=$(OBJ)/%.o): $(2) | $(dir $(2:$(SRC)/%=$(OBJ)/%)) $(dir $(2:$(SRC)/%=.bmake/%))
ifdef VERBOSE
	@set -e; \
	trap 'rm $(2:%.S=%.s)' EXIT; \
	echo $$(AS_CPP_$(1)) $$(AS_CPPFLAGS_$(1)) $$< -o $(2:%.S=%.s); \
	$$(AS_CPP_$(1)) $$(AS_CPPFLAGS_$(1)) $$< -o $(2:%.S=%.s); \
	echo $$(AS_$(1)) $$(ASFLAGS_$(1)) -c $(2:%.S=%.s) -o $$@; \
	$$(AS_$(1)) $$(ASFLAGS_$(1)) -c $(2:%.S=%.s) -o $$@
else
	@set -e; \
	trap 'rm $(2:%.S=%.s)' EXIT; \
	echo 'CPP     $(2:$(SRC)/%.S=%.s)'; \
	$$(AS_CPP_$(1)) $$(AS_CPPFLAGS_$(1)) $$< -o $(2:%.S=%.s); \
	echo 'AS      $$(@:$(OBJ)/%=%)'; \
	$$(AS_$(1)) $$(ASFLAGS_$(1)) -c $(2:%.S=%.s) -o $$@
endif
	@echo $$(AS_CPP_$(1)) $$(AS_CPPFLAGS_$(1)) '&' $$(AS_$(1)) $$(ASFLAGS_$(1)) >$(2:$(SRC)/%=.bmake/%.b)

.PHONY: $$(shell echo $$(AS_CPP_$(1)) $$(AS_CPPFLAGS_$(1)) '&' $$(AS_$(1)) $$(ASFLAGS_$(1)) | diff - $(2:$(SRC)/%=.bmake/%.b) 2>/dev/null || echo $(2:$(SRC)/%=$(OBJ)/%.o))
.INTERMEDIATE: $(2:%.S=%.s)

endef

define TARGET_TEMPLATE
OBJS_$(1)               := $(SRCS_$(1):$(SRC)/%=$(OBJ)/%.o)

$(1).bin: $$(OBJS_$(1))
ifdef VERBOSE
	$$(LD_$(1)) $$(LDFLAGS_$(1)) $$(OBJS_$(1)) -o $$@
	@echo $$(LD_$(1)) $$(LDFLAGS_$(1)) >$(1:%=.bmake/%.bin.b)
else
	@echo 'LD      $$@'
	@$$(LD_$(1)) $$(LDFLAGS_$(1)) $$(OBJS_$(1)) -o $$@
	@echo $$(LD_$(1)) $$(LDFLAGS_$(1)) >$(1:%=.bmake/%.bin.b)
endif

.PHONY: $$(shell echo $$(LD_$(1)) $$(LDFLAGS_$(1)) | diff - $(1:%=.bmake/%.bin.b) 2>/dev/null || echo $(1).bin)

$(foreach f,$(filter %.c,$(SRCS_$(1))),$(eval $(call C_FILE_TEMPLATE,$(1),$(f))))
$(foreach f,$(filter %.s,$(SRCS_$(1))),$(eval $(call AS_FILE_TEMPLATE,$(1),$(f))))
$(foreach f,$(filter %.S,$(SRCS_$(1))),$(eval $(call AS_CPP_FILE_TEMPLATE,$(1),$(f))))
endef

$(IMAGE): $(TARGETS:%=%.bin) $(SRC)/grub.cfg | $(ISO) $(ISO)/boot $(ISO)/boot/grub
ifdef VERBOSE
	cp $(TARGETS:%=%.bin) $(ISO)/boot
	cp $(SRC)/grub.cfg $(ISO)/boot/grub
	grub-mkrescue -o $@ $(ISO) >/dev/null 2>/dev/null
else
	@echo 'GRUB    $@'
	@cp $(TARGETS:%=%.bin) $(ISO)/boot
	@cp $(SRC)/grub.cfg $(ISO)/boot/grub
	@grub-mkrescue -o $@ $(ISO) >/dev/null 2>/dev/null
endif

$(foreach t,$(TARGETS),$(eval $(call TARGET_TEMPLATE,$(t))))

$(sort $(DIRS)):
ifdef VERBOSE
	mkdir -p $@
else
	@mkdir -p $@
endif

$(sort $(QUIET_DIRS)):
	@mkdir -p $@

clean:
ifdef VERBOSE
	rm -rf $(IMAGE) $(TARGETS:%=%.bin) $(ISO) $(OBJ) $(DIST).tar.bz2 .bmake
else
	@rm -rf $(IMAGE) $(TARGETS:%=%.bin) $(ISO) $(OBJ) $(DIST).tar.bz2 .bmake
endif

dist: clean
ifdef VERBOSE
	tar -cvf $(DIST).tar --exclude $(DIST).tar --exclude-vcs --exclude-vcs-ignores .
	$(BZIP2) $(DIST).tar
else
	@echo 'TAR     $(DIST)'
	@tar -cf $(DIST).tar --exclude $(DIST).tar --exclude-vcs --exclude-vcs-ignores .
	@echo 'BZIP2   $(DIST).tar'
	@$(BZIP2) $(DIST).tar
endif

run: all
ifdef VERBOSE
	qemu-system-x86_64 -drive format=raw,file=$(IMAGE)
else
	@qemu-system-x86_64 -drive format=raw,file=$(IMAGE)
endif

run-debug: all
ifdef VERBOSE
	qemu-system-x86_64 -no-shutdown -no-reboot -d int -drive format=raw,file=$(IMAGE)
else
	@qemu-system-x86_64 -no-shutdown -no-reboot -d int -drive format=raw,file=$(IMAGE)
endif

todo-list:
	@find src -type f | xargs cat | grep -F $$'TODO\nFIXME' | sed -E -e 's:(/\*|\*/)::g' -e 's/^ *//' -e 's/ *$$//'

.PHONY: all clean dist run run-debug todo-list

-include $(DEPS)
