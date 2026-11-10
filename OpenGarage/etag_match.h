#pragma once
#include <stddef.h>
#include <string.h>

// If-None-Match evaluation (RFC 9110 13.1.2): true when the field value is
// "*" or a well-formed list of entity-tags containing `tag` under weak
// comparison (an optional W/ prefix is ignored). `tag` is the quoted opaque
// tag, e.g. "\"0123456789abcdef\"". A malformed value never matches, so the
// caller falls back to a full 200. No allocation; `value` must be NUL-terminated.
inline bool etag_list_matches(const char *value, const char *tag) {
	auto ows = [](const char *&p) { while (*p == ' ' || *p == '\t') ++p; };
	const char *p = value;
	ows(p);
	if (*p == '*') { ++p; ows(p); return *p == 0; }
	const size_t tag_len = strlen(tag);
	bool found = false, any = false;
	for (;;) {
		ows(p);
		if (!*p) return any && found;
		if (*p == ',') { ++p; continue; }            // empty list elements are allowed
		if (p[0] == 'W' && p[1] == '/') p += 2;
		if (*p != '"') return false;
		const char *start = p++;
		for (; *p != '"'; ++p) {
			const unsigned char c = (unsigned char)*p;
			// etagc = %x21 / %x23-7E / obs-text; NUL, controls, space and DEL end it as malformed.
			if (!(c == 0x21 || (c >= 0x23 && c != 0x7f))) return false;
		}
		++p;                                          // closing quote
		if ((size_t)(p - start) == tag_len && memcmp(start, tag, tag_len) == 0) found = true;
		any = true;
		ows(p);
		if (*p && *p != ',') return false;            // junk after a tag
	}
}
