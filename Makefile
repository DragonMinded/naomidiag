# Please see the README for setting up a valid build environment.

# The top-level binary that you wish to produce.
all: naomidiag.bin

# Main executable, control reading, screen code.
SRCS += main.c
SRCS += controls.c
SRCS += screens.c

# Our system font for all screens.
SRCS += dejavusans.ttf

# Sounds for menu navigation and audio test.
SRCS += scroll.raw

# Graphics for cursor and up/down scroll indicators.
SRCS += up.png
SRCS += dn.png
SRCS += cursor.png
SRCS += pswoff.png
SRCS += pswon.png

# Libraries we need to link against.
LIBS += -lnaomisprite -lfreetype -lbz2 -lz -lpng16

# Override the default serial so we have our own settings.
SERIAL = BND0

# Pick up base makefile rules common to all examples.
include ${NAOMI_BASE}/tools/Makefile.base

# Specific buildrule for PNG files for this project. Note that these
# *MUST* be a multiple of 8 both in width and height due to TA
# texture limitations.
build/%.o: %.png ${IMG2C_FILE}
	@mkdir -p $(dir $@)
	${IMG2C} build/$<.c --mode RGBA1555 $<
	${CC} -c build/$<.c -o $@

# Config for our top-level ROM, including name and publisher.
naomidiag.bin: ${MAKEROM_FILE} ${NAOMI_BIN_FILE}
	${MAKEROM} $@ \
		--title "Naomi Diagnostic Test ROM" \
		--publisher "DragonMinded" \
		--serial "${SERIAL}" \
		--section ${NAOMI_BIN_FILE},${START_ADDR} \
		--entrypoint ${MAIN_ADDR} \
		--main-binary-includes-test-binary \
		--test-entrypoint ${TEST_ADDR}

# Our clean target, which wipes the built target. Note that
# we check in naomidiag.bin so people can download it from
# github, so a make clean should be followed by a make.
.PHONY: clean
clean:
	rm -rf build
	rm -rf naomidiag.bin
