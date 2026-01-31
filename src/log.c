#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static int log_level;

static const char *_level_tags[] = {
	"FFF",
	"CR!",
	"ERR",
	"WRN",
	"INF",
	"DBG"
};

void log_set_level(const int level)
{
	if (level < LOG_CRITICAL) {
		log_level = LOG_CRITICAL;
	} else if (level > LOG_DEBUG) {
		log_level = LOG_DEBUG;
	} else {
		log_level = level;
	}
}

static int get_timestamp(char *dst, const size_t dst_size)
{
	time_t now;
	struct tm thetime;

	now = time(NULL);

	if (!localtime_r(&now, &thetime)) {
		return -errno;
	}

	return strftime(dst, dst_size, "%Y-%m-%d %H:%M:%S %Z",
	                &thetime);
}

int log_write(const int level, const char *tag,
              const char *file, const int line,
              const char *format, ...)
{
	int err;
	int eff_lvl;
	va_list args;

	err = 0;

	if (level < LOG_FORCE) {
		eff_lvl = LOG_FORCE;
	} else if (level > LOG_DEBUG) {
		eff_lvl = LOG_DEBUG;
	} else {
		eff_lvl = level;
	}

	if (eff_lvl <= log_level) {
		char timestamp[32];
		const char *lvl_tag;

		err = get_timestamp(timestamp, sizeof(timestamp));
		if (err < 0) {
			return err;
		}

		lvl_tag = _level_tags[eff_lvl + 1];

		if (file) {
			err = fprintf(stderr, "%s [%s:%s] %s:%d ",
			              timestamp, tag, lvl_tag, file, line);
		} else {
			err = fprintf(stderr, "%s [%s:%s] ",
			              timestamp, tag, lvl_tag);
		}

		if (err < 0) {
			err = -errno;
		} else {
			va_start(args, format);
			if (vfprintf(stderr, format, args) < 0) {
				err = -errno;
			}
			va_end(args);

			if (err >= 0) {
				if (fprintf(stderr, "\n") < 0) {
					err = -errno;
				}
			}
		}
	}

	return err;
}
