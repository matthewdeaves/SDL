/* setdepth [bpp] -- print, or switch the main display's colour depth at its
 * current resolution for this login session only (kCGConfigureForSession,
 * reverted at logout). Test tool for matthewdeaves/SDL#5; 10.3 APIs only.
 *
 * Usage: setdepth        -> prints e.g. "800x600 32bpp"
 *        setdepth 16     -> Thousands of colors; setdepth 32 -> Millions
 * Run over ssh as the logged-in console user (verified on Panther 10.3.9).
 * Restore the original depth afterwards.
 *
 * Build (Lion build mini, generic ppc, 10.3 floor):
 *   /usr/bin/gcc-4.0 -arch ppc -isysroot /Developer/SDKs/MacOSX10.3.9.sdk \
 *     -mmacosx-version-min=10.3 -O2 -Wl,-syslibroot,/Developer/SDKs/MacOSX10.3.9.sdk \
 *     -o setdepth setdepth.c -framework ApplicationServices
 * A binary built this way (sha256 1d692b6f...) is attached to the
 * matthewdeaves/SDL-1.2 release retro/panther-ppc-sdl5-fix. */
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>

static void show(CGDirectDisplayID d)
{
	printf("%lux%lu %lubpp\n", (unsigned long)CGDisplayPixelsWide(d),
	       (unsigned long)CGDisplayPixelsHigh(d),
	       (unsigned long)CGDisplayBitsPerPixel(d));
}

int main(int argc, char **argv)
{
	CGDirectDisplayID d = CGMainDisplayID();
	CFDictionaryRef mode;
	CGDisplayConfigRef cfg;
	boolean_t exact = 0;
	size_t bpp;
	CGError e;

	if (argc < 2) {
		show(d);
		return 0;
	}
	bpp = (size_t)atoi(argv[1]);
	mode = CGDisplayBestModeForParameters(d, bpp, CGDisplayPixelsWide(d),
	                                      CGDisplayPixelsHigh(d), &exact);
	if (mode == NULL || !exact) {
		fprintf(stderr, "setdepth: no exact %lubpp mode at current size\n",
		        (unsigned long)bpp);
		return 2;
	}
	if ((e = CGBeginDisplayConfiguration(&cfg)) != kCGErrorSuccess) {
		fprintf(stderr, "setdepth: begin failed %d\n", (int)e);
		return 3;
	}
	CGConfigureDisplayMode(cfg, d, mode);
	if ((e = CGCompleteDisplayConfiguration(cfg, kCGConfigureForSession)) != kCGErrorSuccess) {
		fprintf(stderr, "setdepth: complete failed %d\n", (int)e);
		return 4;
	}
	show(d);
	return 0;
}
