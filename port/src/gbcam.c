#include <stdio.h>
#include <string.h>
#include <math.h>
#include <PR/ultratypes.h>
#include "versions.h"
#include "platform.h"
#include "config.h"
#include "system.h"
#include "fs.h"
#include "gbcam.h"
#include "gbcamsrc.h"

#if PERFECTHEAD

/**
 * Virtual Game Boy Camera (MAC-GBD mapper + M64282FP sensor).
 *
 * References:
 * - Pan Docs, "Game Boy Camera": https://gbdev.io/pandocs/Gameboy_Camera.html
 * - GiiBiiAdvance (sample implementation referenced by Pan Docs)
 * - MAME src/devices/bus/gameboy/camera.cpp
 *
 * The analog part of the sensor is approximated (see gbcamSensorValue); the
 * digital parts (registers, dither matrix, tile format, SRAM layout, capture
 * busy flag and timing) follow the documentation.
 */

#define GBCAM_SRAM_SIZE   0x20000
#define GBCAM_SRAM_FNAME  "$S/gbcam.sav"
#define GBCAM_NUM_REGS    0x36
#define GBCAM_IMAGE_OFS   0x0100 // last capture, SRAM bank 0
#define GBCAM_ALBUM_OFS   0x11b2 // album state vector (30 slots, 0xff = empty)
#define GBCAM_ALBUM_SLOTS 30

// exposure the game uses by default (camdraw.c var8007f904[30]); used as the
// "normal" brightness point of the approximated sensor
#define GBCAM_EXPOSURE_REF 0x085a
#define GBCAM_GAIN_REF_DB  23.0f

static s32 gbcamController = 0; // config: 0 = disabled, 1-4 = controller port

static u8 *sram;
static u8 sramDirty;
static u8 regs[GBCAM_NUM_REGS];
static u8 ramEnabled;
static u8 ramBank; // bit 4 set = camera registers mapped at A000

static u8 capturing;
static u64 captureEndUs;
static u8 frame[GBCAMSRC_WIDTH * GBCAMSRC_HEIGHT];

// analog gain in dB for register 1 bits 0-3 (Pan Docs; the G=0 entry is not
// documented, so it is extrapolated). Bit 4 adds 6 dB.
static const f32 gainDb[16] = {
	12.5f, 14.0f, 17.0f, 18.5f, 20.0f, 21.5f, 23.0f, 24.5f,
	26.0f, 29.0f, 32.0f, 35.0f, 38.0f, 41.0f, 45.5f, 51.5f,
};

static const f32 edgeAlpha[8] = { 0.50f, 0.75f, 1.00f, 1.25f, 2.00f, 3.00f, 4.00f, 5.00f };

s32 gbcamGetChannel(void)
{
	return gbcamController - 1;
}

static void gbcamSramSave(void)
{
	FILE *f;

	if (!sram || !sramDirty) {
		return;
	}

	f = fsFileOpenWrite(GBCAM_SRAM_FNAME);

	if (f) {
		fwrite(sram, 1, GBCAM_SRAM_SIZE, f);
		fsFileFree(f);
		sramDirty = 0;
	} else {
		sysLogPrintf(LOG_ERROR, "gbcam: could not save SRAM to %s", fsFullPath(GBCAM_SRAM_FNAME));
	}
}

static void gbcamSramInit(void)
{
	if (sram) {
		return;
	}

	sram = sysMemZeroAlloc(GBCAM_SRAM_SIZE);

	if (!sram) {
		sysFatalError("gbcam: could not allocate SRAM");
	}

	// a real camera save (e.g. dumped from a cartridge) can be dropped in here
	if (fsFileSize(GBCAM_SRAM_FNAME) == GBCAM_SRAM_SIZE
			&& fsFileLoadTo(GBCAM_SRAM_FNAME, sram, GBCAM_SRAM_SIZE) > 0) {
		sysLogPrintf(LOG_NOTE, "gbcam: loaded SRAM from %s", fsFullPath(GBCAM_SRAM_FNAME));
	} else {
		memset(sram + GBCAM_ALBUM_OFS, 0xff, GBCAM_ALBUM_SLOTS);
	}
}

/**
 * Approximate the sensor: luma (0-255) -> unsigned sensor value (0-255) as
 * compared against the dither matrix thresholds.
 *
 * Perfect Dark's matrices (pak0f11d214) put their thresholds between 128 and
 * 255, so black maps to the 128 reference level and full brightness at the
 * default exposure/gain maps to 255. Exposure scales linearly and gain by dB.
 */
static s32 gbcamSensorValue(s32 luma, f32 scale)
{
	s32 v = 128 + (s32)(luma * scale * 0.5f);
	return v > 255 ? 255 : v;
}

static void gbcamProcessFrame(void)
{
	static s16 sensor[GBCAMSRC_WIDTH * GBCAMSRC_HEIGHT];
	const u32 exposure = (regs[2] << 8) | regs[3];
	const s32 gain = regs[1] & 0x1f;
	const s32 vh = (regs[1] >> 5) & 3;
	const s32 invert = regs[4] & 0x08;
	const f32 alpha = edgeAlpha[(regs[4] >> 4) & 7];
	const f32 db = gainDb[gain & 0xf] + ((gain & 0x10) ? 6.0f : 0.0f);
	const f32 scale = ((f32)exposure / GBCAM_EXPOSURE_REF) * powf(10.0f, (db - GBCAM_GAIN_REF_DB) / 20.0f);
	u8 *tiles = sram + GBCAM_IMAGE_OFS;
	s32 x, y;

	if (!gbcamsrcGetFrame(frame)) {
		memset(frame, 0, sizeof(frame));
	}

	// sensor stage, kept signed around the 128 reference level
	for (y = 0; y < GBCAMSRC_HEIGHT; y++) {
		for (x = 0; x < GBCAMSRC_WIDTH; x++) {
			s32 v = gbcamSensorValue(frame[y * GBCAMSRC_WIDTH + x], scale);

			if (invert) {
				v = 255 - v;
			}

			sensor[y * GBCAMSRC_WIDTH + x] = v - 128;
		}
	}

	memset(tiles, 0, 16 * 14 * 16);

	for (y = 0; y < GBCAMSRC_HEIGHT; y++) {
		for (x = 0; x < GBCAMSRC_WIDTH; x++) {
			const s32 i = y * GBCAMSRC_WIDTH + x;
			const s32 n = y > 0 ? sensor[i - GBCAMSRC_WIDTH] : sensor[i];
			const s32 s = y < GBCAMSRC_HEIGHT - 1 ? sensor[i + GBCAMSRC_WIDTH] : sensor[i];
			const s32 w = x > 0 ? sensor[i - 1] : sensor[i];
			const s32 e = x < GBCAMSRC_WIDTH - 1 ? sensor[i + 1] : sensor[i];
			const u8 *thr = &regs[6 + ((y & 3) * 4 + (x & 3)) * 3];
			f32 fv = sensor[i];
			s32 v, colour;
			u8 *row;

			// edge enhancement
			if (vh == 3) {
				fv += alpha * (4 * sensor[i] - n - s - w - e);
			} else if (vh == 2) {
				fv += alpha * (2 * sensor[i] - w - e);
			} else if (vh == 1) {
				fv += alpha * (2 * sensor[i] - n - s);
			}

			v = (s32)fv;
			v = (v < -128 ? -128 : (v > 127 ? 127 : v)) + 128;

			// dither matrix: 0 = lightest .. 3 = darkest
			if (v < thr[0]) {
				colour = 3;
			} else if (v < thr[1]) {
				colour = 2;
			} else if (v < thr[2]) {
				colour = 1;
			} else {
				colour = 0;
			}

			row = &tiles[((y >> 3) * 16 + (x >> 3)) * 16 + (y & 7) * 2];

			if (colour & 1) {
				row[0] |= 0x80 >> (x & 7);
			}

			if (colour & 2) {
				row[1] |= 0x80 >> (x & 7);
			}
		}
	}
}

static void gbcamUpdate(void)
{
	if (capturing && sysGetMicroseconds() >= captureEndUs) {
		gbcamProcessFrame();
		capturing = 0;
		regs[0] &= ~1;
	}
}

static void gbcamStartCapture(void)
{
	// Pan Docs: 32446 + (N ? 0 : 512) + 16 * exposure cycles at 1 MiHz
	const u32 exposure = (regs[2] << 8) | regs[3];
	const u64 cycles = 32446 + ((regs[1] & 0x80) ? 0 : 512) + 16ull * exposure;

	capturing = 1;
	captureEndUs = sysGetMicroseconds() + cycles * 1000000ull / 1048576ull;
}

void gbcamGetId(OSGbpakId *id)
{
	memset(id, 0, sizeof(*id));
	memcpy(id->game_title, "GAMEBOYCAMERA", 13);
	id->company_code = 0x3031; // "01"
	id->cart_type = 0xfc; // POCKET CAMERA
	id->rom_size = 0x05; // 1 MiB
	id->ram_size = 0x04; // 128 KiB
	id->country_code = 0x01;
	id->fixed2 = 0x33;
}

s32 gbcamRead(u16 addr, u8 *buf, u16 size)
{
	u16 i;

	gbcamSramInit();
	gbcamUpdate();

	for (i = 0; i < size; i++, addr++) {
		u8 val = 0;

		if (addr >= 0xa000 && addr < 0xc000) {
			if (ramBank & 0x10) {
				const u16 reg = (addr - 0xa000) & 0x7f;
				// only A000 is readable: bits 0-2, bit 0 = capture busy
				val = reg == 0 ? (regs[0] & 7) : 0;
			} else if (!capturing) {
				val = sram[(ramBank & 0x0f) * 0x2000 + (addr - 0xa000)];
			}
		}

		buf[i] = val;
	}

	return 0;
}

s32 gbcamWrite(u16 addr, const u8 *buf, u16 size)
{
	u8 trigger = 0;
	u16 i;

	gbcamSramInit();
	gbcamUpdate();

	for (i = 0; i < size; i++, addr++) {
		const u8 val = buf[i];

		if (addr < 0x2000) {
			ramEnabled = (val & 0x0f) == 0x0a;
		} else if (addr >= 0x4000 && addr < 0x6000) {
			ramBank = val;
		} else if (addr >= 0xa000 && addr < 0xc000) {
			if (ramBank & 0x10) {
				const u16 reg = (addr - 0xa000) & 0x7f;

				if (reg == 0) {
					regs[0] = val & 7;
					trigger = val & 1;

					if (!trigger) {
						capturing = 0;
					}
				} else if (reg < GBCAM_NUM_REGS) {
					regs[reg] = val;
				}
			} else if (ramEnabled && !capturing) {
				sram[(ramBank & 0x0f) * 0x2000 + (addr - 0xa000)] = val;
				sramDirty = 1;
			}
		}
	}

	// The game writes A000 (trigger) and the sensor/matrix registers in one
	// 64-byte block, trigger first. Start the capture once the whole block has
	// landed so it uses the new parameters.
	if (trigger && !capturing) {
		gbcamStartCapture();
	}

	gbcamSramSave();

	return 0;
}

PD_CONSTRUCTOR static void gbcamConfigInit(void)
{
	configRegisterInt("PerfectHead.VirtualCameraController", &gbcamController, 0, 4);
}

#else

s32 gbcamGetChannel(void)
{
	return -1;
}

s32 gbcamRead(u16 addr, u8 *buf, u16 size)
{
	return -1;
}

s32 gbcamWrite(u16 addr, const u8 *buf, u16 size)
{
	return -1;
}

void gbcamGetId(OSGbpakId *id)
{
	memset(id, 0, sizeof(*id));
}

#endif
