ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set)
endif

include $(DEVKITARM)/3ds_rules

TARGET := ShieldUpdater
BUILD := build
APP_TITLE := Shield Updater
APP_AUTHOR := Yanis
APP_DESCRIPTION := Shield app updates / Mises a jour des apps Shield
CIA_VERSION := 28
MAKEROM ?= makerom
BANNERTOOL ?= bannertool
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -O2 -g -Wall -Wextra -mword-relocations -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += -Iinclude -I$(DEVKITPRO)/libctru/include -I$(DEVKITPRO)/portlibs/3ds/include -D__3DS__
CFLAGS += -DSHIELD_UPDATER_CIA_VERSION=$(CIA_VERSION)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS := -L$(DEVKITPRO)/portlibs/3ds/lib -L$(DEVKITPRO)/libctru/lib -lcurl -ljansson -lmbedtls -lmbedx509 -lmbedcrypto -lvorbisidec -logg -lz -lctru -lm

.PHONY: all cia clean
MUSIC_FILES := $(wildcard romfs/music/*)
all: cia
cia: $(BUILD)/$(TARGET).cia

$(BUILD)/$(TARGET).cia: $(BUILD)/$(TARGET).elf romfs/background.bgr romfs/boot-bottom.bgr romfs/updater-icon.rgb romfs/font.bin romfs/app-icons.rgb romfs/ca-bundle.pem $(MUSIC_FILES) include/minimp3.h include/minimp3_ex.h Makefile app.rsf icon.png banner.png assets/silence.wav $(MAKEROM) $(BANNERTOOL)
	@"$(BANNERTOOL)" makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i icon.png -o $(BUILD)/icon.icn
	@"$(BANNERTOOL)" makebanner -i banner.png -a assets/silence.wav -o $(BUILD)/banner.bnr
	@"$(MAKEROM)" -f cia -o "$@.tmp" -target t -ver $(CIA_VERSION) -desc app:4 -rsf app.rsf -exefslogo -elf $(BUILD)/$(TARGET).elf -icon $(BUILD)/icon.icn -banner $(BUILD)/banner.bnr
	@mv -f "$@.tmp" "$@"

$(BUILD)/$(TARGET).elf: source/main.c source/music.c source/music.h include/minimp3.h include/minimp3_ex.h romfs/background.bgr romfs/boot-bottom.bgr romfs/updater-icon.rgb romfs/font.bin romfs/app-icons.rgb romfs/ca-bundle.pem $(MUSIC_FILES) Makefile
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o "$@" source/main.c source/music.c $(LIBS)


clean:
	@rm -rf $(BUILD)
