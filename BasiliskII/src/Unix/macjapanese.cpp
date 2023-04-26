#include <iconv.h>

#include "sysdeps.h"

const char *utf8_to_macjapanese(const char *utf8, size_t utf8_len)
{
	// iconv does not have support for MACJAPANESE, so decode to SHIFT_JIS.
	iconv_t iconv_cd = iconv_open("SHIFT_JIS", "UTF-8");
	if (iconv_cd == (iconv_t) -1) {
		printf("Could not get UTF-8 -> SHIFT_JIS conversion descriptor\n");
		return utf8;
	}

	char *utf8_in = const_cast<char *>(utf8);
	size_t utf8_in_len = utf8_len;
	size_t shiftjis_len = utf8_len; // upper bound
	char *shiftjis = (char *)malloc(shiftjis_len + 1);
	size_t shiftjis_out_len = shiftjis_len;
	char *shiftjis_out = shiftjis;
	if (iconv(iconv_cd, &utf8_in, &utf8_in_len, &shiftjis_out, &shiftjis_out_len) == -1) {
		printf("Could not run UTF-8 -> Shift JIS conversion for %s\n", utf8);
		iconv_close(iconv_cd);
		free(shiftjis);
		return utf8;
	}
	shiftjis_len -= shiftjis_out_len;
	iconv_close(iconv_cd);

	shiftjis[shiftjis_len] = '\0';

	return shiftjis;
}

const char *macjapanese_to_utf8(const char *macjapanese, size_t macjapanese_len) {
	iconv_t iconv_cd = iconv_open("UTF-8", "SHIFT_JIS");
	if (iconv_cd == (iconv_t) -1) {
		printf("Could not get SHIFT_JIS -> UTF-8 conversion descriptor\n");
		return macjapanese;
	}

	char *macjapanese_in = const_cast<char *>(macjapanese);
	size_t utf8_len = macjapanese_len * 3; // upper bound
	char *utf8 = (char *)malloc(utf8_len + 1);
	size_t utf8_out_len = utf8_len;
	char *utf8_out = utf8;
	if (iconv(iconv_cd, &macjapanese_in, &macjapanese_len, &utf8_out, &utf8_out_len) == -1) {
		printf("Could not run Shift JIS -> UTF-8 conversion for %s\n", macjapanese);
		iconv_close(iconv_cd);
		free(utf8);
		return macjapanese;
	}
	utf8_len -= utf8_out_len;
	iconv_close(iconv_cd);

	utf8[utf8_len] = '\0';

	return utf8;
}
