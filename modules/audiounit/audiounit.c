/**
 * @file audiounit.c  AudioUnit sound driver
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "audiounit.h"


/**
 * @defgroup audiounit audiounit
 *
 * Audio driver module for OSX/iOS AudioUnit
 */


AudioComponent audiounit_comp = NULL;

static struct auplay *auplay;
static struct ausrc *ausrc;


uint32_t audiounit_aufmt_to_formatflags(enum aufmt fmt)
{
	switch (fmt) {

	case AUFMT_S16LE:  return kLinearPCMFormatFlagIsSignedInteger;
	case AUFMT_FLOAT:  return kLinearPCMFormatFlagIsFloat;
	default: return 0;
	}
}


AudioDeviceID audiounit_default_device(bool recording)
{

	AudioObjectPropertyAddress auAddress = {
		recording
		? kAudioHardwarePropertyDefaultInputDevice
		: kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioDeviceID device_id;
	UInt32 ausize = sizeof(device_id);
	OSStatus ret = 0;

	ret = AudioObjectGetPropertyData(kAudioObjectSystemObject,
			&auAddress,
			0,
			NULL,
			&ausize,
			&device_id);
	if (ret)
		return 0;

	return device_id;
}


int audiounit_print_samplerates(AudioDeviceID device)
{
	AudioValueRange rangev[64];
	uint32_t count, i;
	uint32_t size;
	OSStatus ret;

	const AudioObjectPropertyAddress prop_addr = {
		kAudioDevicePropertyAvailableNominalSampleRates,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	size = sizeof(rangev);

	ret = AudioObjectGetPropertyData(device,
					 &prop_addr,
					 0,
					 NULL,
					 &size,
					 rangev);
	if (ret)
		return ENODEV;


	count = size / sizeof(rangev[0]);

	re_printf("Available %d Sample Rate\n", count);

	for (i=0; i<count; i++) {

		re_printf("Available Sample Rate value : %.1f - %.1f\n",
			  rangev[i].mMinimum, rangev[i].mMaximum);
	}

	re_printf("\n");

	return 0;
}


#if TARGET_OS_IPHONE
static void interruptionListener(void *data, UInt32 inInterruptionState)
{
	(void)data;

	if (inInterruptionState == kAudioSessionBeginInterruption) {
		info("audiounit: interrupt Begin\n");
		audiosess_interrupt(true);
	}
	else if (inInterruptionState == kAudioSessionEndInterruption) {
		info("audiounit: interrupt End\n");
		audiosess_interrupt(false);
	}
}
#endif


static int module_init(void)
{
	AudioComponentDescription desc;
	CFStringRef name = NULL;
	int err;

#if TARGET_OS_IPHONE
	OSStatus ret;

	ret = AudioSessionInitialize(NULL, NULL, interruptionListener, 0);
	if (ret && ret != kAudioSessionAlreadyInitialized) {
		warning("audiounit: AudioSessionInitialize: %d\n", ret);
		return ENODEV;
	}
#endif

	desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
	desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
#else
	desc.componentSubType = kAudioUnitSubType_HALOutput;
#endif
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	audiounit_comp = AudioComponentFindNext(NULL, &desc);
	if (!audiounit_comp) {
		warning("audiounit: Voice Processing I/O not found\n");
		return ENOENT;
	}

	if (0 == AudioComponentCopyName(audiounit_comp, &name)) {
		debug("audiounit: using component '%s'\n",
		      CFStringGetCStringPtr(name, kCFStringEncodingUTF8));
	}

	err  = auplay_register(&auplay, baresip_auplayl(),
			       "audiounit", audiounit_player_alloc);
	err |= ausrc_register(&ausrc, baresip_ausrcl(),
			      "audiounit", audiounit_recorder_alloc);

	info("audiounit: recording device: %u\n",
	     audiounit_default_device(true));
	audiounit_print_samplerates(audiounit_default_device(true));

	info("audiounit: playback device: %u\n",
	     audiounit_default_device(false));
	audiounit_print_samplerates(audiounit_default_device(false));

	return err;
}


static int module_close(void)
{
	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(audiounit) = {
	"audiounit",
	"audio",
	module_init,
	module_close,
};
