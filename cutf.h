/*
Copyright 2014, Jason Kozak
Based on UTF8-CPP: Copyright 2006, Nemanja Trifunovicm

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
#ifndef CUTF_H
#define CUTF_H

#include <stdint.h>
#include <stddef.h>

int      cutf_is_valid(uint8_t* start, uint8_t* end);
int      cutf_starts_with_bom(uint8_t* start, uint8_t* end);
uint8_t* cutf_append(uint32_t cp, uint8_t* result);
uint32_t cutf_next(uint8_t** it);
uint32_t cutf_peek_next(uint8_t* it);
uint32_t cutf_prior(uint8_t** it);
void     cutf_advance(uint8_t** it, size_t n);
size_t   cutf_distance(uint8_t* first, uint8_t* last);
size_t   cutf_16to8(uint16_t* start, uint16_t* end, uint8_t* out);
size_t   cutf_8to16(uint8_t* start, uint8_t* end, uint16_t* out);
size_t   cutf_32to8(uint32_t* start, uint32_t* end, uint8_t* out);
size_t   cutf_8to32(uint8_t* start, uint8_t* end, uint32_t* out);
size_t   cutf_replace_invalid(uint8_t* start, uint8_t* end, uint8_t* out, size_t limit, uint32_t replacement);
size_t   cutf_default_replace_invalid(uint8_t* start, uint8_t* end, uint8_t* out, size_t limit);

#if CUTF_IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////
// internal
////////////////////////////////////////////////////////////////////////////////
#define CUTF_LEAD_SURROGATE_MIN  0xd800u
#define CUTF_LEAD_SURROGATE_MAX  0xdbffu
#define CUTF_TRAIL_SURROGATE_MIN 0xdc00u
#define CUTF_TRAIL_SURROGATE_MAX 0xdfffu
#define CUTF_LEAD_OFFSET         (CUTF_LEAD_SURROGATE_MIN - (0x10000 >> 10))
#define CUTF_SURROGATE_OFFSET    (0x10000u - (CUTF_LEAD_SURROGATE_MIN << 10) - CUTF_TRAIL_SURROGATE_MIN)
#define CUTF_CODE_POINT_MAX      0x0010ffffu // Maximum valid value for a Unicode code point

#define CUTF_MASK8(oc)               ((uint8_t)(0xff & (oc)))
#define CUTF_MASK16(oc)              ((uint16_t)(0xffff & (oc)))
#define CUTF_IS_TRAIL(oc)            ((CUTF_MASK8(oc) >> 6) == 0x2)
#define CUTF_IS_LEAD_SURROGATE(cp)   ((cp) >= CUTF_LEAD_SURROGATE_MIN && (cp) <= CUTF_LEAD_SURROGATE_MAX)
#define CUTF_IS_TRAIL_SURROGATE(cp)  ((cp) >= CUTF_TRAIL_SURROGATE_MIN && (cp) <= CUTF_TRAIL_SURROGATE_MAX)
#define CUTF_IS_SURROGATE(cp)        ((cp) >= CUTF_LEAD_SURROGATE_MIN && (cp) <= CUTF_TRAIL_SURROGATE_MAX)
#define CUTF_IS_CODE_POINT_VALID(cp) ((cp) <= CUTF_CODE_POINT_MAX && !CUTF_IS_SURROGATE(cp))
#define CUTF_INC_OR_RETURN(IT, END)  {enum cutf_error ret = cutf_safe_inc((IT),(END)); if(ret != CUTF_OK) return ret;}
#define CUTF_INC_IT_PTR(it)          (++(*it))
#define CUTF_DEC_IT_PTR(it)          (--(*it))

enum cutf_error
{
	CUTF_OK,
	CUTF_INVALID_LEAD,
	CUTF_TRUNCATED_SEQUENCE,
	CUTF_INCOMPLETE_SEQUENCE,
	CUTF_OVERLONG_SEQUENCE,
	CUTF_INVALID_CODE_POINT
};

static size_t cutf_codepoint_length(uint32_t cp)
{
	if(cp < 0x80)
		return 1;
	else if(cp < 0x800)
		return 2;
	else if(cp < 0x10000)
		return 3;
	else
		return 4;
}

static size_t cutf_sequence_length(uint8_t* lead_it)
{
	uint8_t lead = CUTF_MASK8(*lead_it);
	if(lead < 0x80) {
		return 1;
	} else if((lead >> 5) == 0x6) {
		return 2;
	} else if((lead >> 4) == 0xe) {
		return 3;
	} else if((lead >> 3) == 0x1e) {
		return 4;
	}
	return 0;
}

static int cutf_is_overlong_sequence(uint32_t cp, size_t length)
{
	if(cp < 0x80) {
		return length != 1;
	} else if(cp < 0x800) {
		return length != 2;
	} else if(cp < 0x10000) {
		return length != 3;
	}
	return 0;
}

static enum cutf_error cutf_safe_inc(uint8_t** it, uint8_t* end)
{
	if(CUTF_INC_IT_PTR(it) == end) {
		return CUTF_TRUNCATED_SEQUENCE;
	} else if(!CUTF_IS_TRAIL(**it)) {
		return CUTF_INCOMPLETE_SEQUENCE;
	}
	return CUTF_OK;
}

/// get_sequence_x functions decode utf-8 sequences of the length x
static enum cutf_error cutf_get_sequence_1(uint8_t** it, uint8_t* end, uint32_t* code_point)
{
	if(*it == end) {
		return CUTF_TRUNCATED_SEQUENCE;
	}
	*code_point = CUTF_MASK8(**it);
	return CUTF_OK;
}

static enum cutf_error cutf_get_sequence_2(uint8_t** it, uint8_t* end, uint32_t* code_point)
{
	if(*it == end) {
		return CUTF_TRUNCATED_SEQUENCE;
	}
	*code_point = CUTF_MASK8(**it);
	CUTF_INC_OR_RETURN(it, end)
	*code_point = ((*code_point << 6) & 0x7ff) + ((**it) & 0x3f);
	return CUTF_OK;
}

static enum cutf_error cutf_get_sequence_3(uint8_t** it, uint8_t* end, uint32_t* code_point)
{
	if(*it == end) {
		return CUTF_TRUNCATED_SEQUENCE;
	}
	*code_point = CUTF_MASK8(**it);
	CUTF_INC_OR_RETURN(it, end)
	*code_point = ((*code_point << 12) & 0xffff) + ((CUTF_MASK8(**it) << 6) & 0xfff);
	CUTF_INC_OR_RETURN(it, end)
	*code_point += (**it) & 0x3f;
	return CUTF_OK;
}

static enum cutf_error cutf_get_sequence_4(uint8_t** it, uint8_t* end, uint32_t* code_point)
{
	if(*it == end) {
	   return CUTF_TRUNCATED_SEQUENCE;
	}
	*code_point = CUTF_MASK8(**it);
	CUTF_INC_OR_RETURN(it, end)
	*code_point = ((*code_point << 18) & 0x1fffff) + ((CUTF_MASK8(**it) << 12) & 0x3ffff);
	CUTF_INC_OR_RETURN(it, end)
	*code_point += (CUTF_MASK8(**it) << 6) & 0xfff;
	CUTF_INC_OR_RETURN(it, end)
	*code_point += (**it) & 0x3f;
	return CUTF_OK;
}

static enum cutf_error cutf_get_next(uint8_t** it, uint8_t* end, uint32_t* code_point)
{
	// Determine the sequence length based on the lead octet
	const size_t length = cutf_sequence_length(*it);
	uint8_t* original_it = *it;
	uint32_t cp = 0;
	enum cutf_error err = CUTF_OK;

	// Get trail octets and calculate the code point
	switch(length) {
		case 0: return CUTF_INVALID_LEAD;
		case 1: err = cutf_get_sequence_1(it, end, &cp); break;
		case 2: err = cutf_get_sequence_2(it, end, &cp); break;
		case 3: err = cutf_get_sequence_3(it, end, &cp); break;
		case 4: err = cutf_get_sequence_4(it, end, &cp); break;
	}

	if(err == CUTF_OK) {
		if(!CUTF_IS_CODE_POINT_VALID(cp)) {
			err = CUTF_INVALID_CODE_POINT;
		} else if(cutf_is_overlong_sequence(cp, length)) {
			err = CUTF_OVERLONG_SEQUENCE;
		} else {
			// Passed! Return here.
			if(code_point) {
				*code_point = cp;
			}
			CUTF_INC_IT_PTR(it);
			return CUTF_OK;
		}
	}
	*it = original_it;
	return err;
}

static enum cutf_error cutf_validate_next(uint8_t** it, uint8_t* end)
{
	return cutf_get_next(it, end, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// API
////////////////////////////////////////////////////////////////////////////////
uint8_t* cutf_find_invalid(uint8_t* start, uint8_t* end)
{
	uint8_t* result = start;
	while(result != end) {
		enum cutf_error err_code = cutf_validate_next(&result, end);
		if(err_code != CUTF_OK)
			return result;
	}
	return result;
}

int cutf_is_valid(uint8_t* start, uint8_t* end)
{
	return (cutf_find_invalid(start, end) == end);
}

int cutf_starts_with_bom(uint8_t* start, uint8_t* end)
{
	const uint8_t bom[] = {0xef, 0xbb, 0xbf};
	return end - start >= 3 && start[0] == bom[0] && start[1] == bom[1] && start[2] == bom[2];
}

uint8_t* cutf_append(uint32_t cp, uint8_t* result)
{
	if(cp < 0x80)                        // one octet
		*(result++) = (uint8_t)(cp);
	else if(cp < 0x800) {                // two octets
		*(result++) = (uint8_t)((cp >> 6)          | 0xc0);
		*(result++) = (uint8_t)((cp & 0x3f)        | 0x80);
	}
	else if(cp < 0x10000) {              // three octets
		*(result++) = (uint8_t)((cp >> 12)         | 0xe0);
		*(result++) = (uint8_t)(((cp >> 6) & 0x3f) | 0x80);
		*(result++) = (uint8_t)((cp & 0x3f)        | 0x80);
	}
	else {                                // four octets
		*(result++) = (uint8_t)((cp >> 18)         | 0xf0);
		*(result++) = (uint8_t)(((cp >> 12) & 0x3f)| 0x80);
		*(result++) = (uint8_t)(((cp >> 6) & 0x3f) | 0x80);
		*(result++) = (uint8_t)((cp & 0x3f)        | 0x80);
	}
	return result;
}

uint32_t cutf_next(uint8_t** it)
{
	uint32_t cp = CUTF_MASK8(**it);
	size_t length = cutf_sequence_length(*it);
	switch (length) {
		case 1:
			break;
		case 2:
			CUTF_INC_IT_PTR(it);
			cp = ((cp << 6) & 0x7ff) + ((**it) & 0x3f);
			break;
		case 3:
			CUTF_INC_IT_PTR(it);
			cp = ((cp << 12) & 0xffff) + ((CUTF_MASK8(**it) << 6) & 0xfff);
			CUTF_INC_IT_PTR(it);
			cp += (**it) & 0x3f;
			break;
		case 4:
			CUTF_INC_IT_PTR(it);
			cp = ((cp << 18) & 0x1fffff) + ((CUTF_MASK8(**it) << 12) & 0x3ffff);
			CUTF_INC_IT_PTR(it);
			cp += (CUTF_MASK8(**it) << 6) & 0xfff;
			CUTF_INC_IT_PTR(it);
			cp += (**it) & 0x3f;
			break;
	}
	CUTF_INC_IT_PTR(it);
	return cp;
}

uint32_t cutf_peek_next(uint8_t* it)
{
	return cutf_next(&it);
}

uint32_t cutf_prior(uint8_t** it)
{
	for(CUTF_DEC_IT_PTR(it); CUTF_IS_TRAIL(**it); CUTF_DEC_IT_PTR(it)) {}
	return cutf_peek_next(*it);
}

void cutf_advance(uint8_t** it, size_t n)
{
	size_t i;
	for(i = 0; i < n; ++i) {
		cutf_next(it);
	}
}

size_t cutf_distance(uint8_t* first, uint8_t* last)
{
	size_t dist;
	for(dist = 0; first < last; ++dist) {
		cutf_next(&first);
	}
	return dist;
}

size_t cutf_16to8(uint16_t* start, uint16_t* end, uint8_t* out)
{
	uint8_t* it = out;
	while(start != end) {
		uint32_t cp = CUTF_MASK16(*start);
		++start;
		// Take care of surrogate pairs first
		if(CUTF_IS_LEAD_SURROGATE(cp)) {
			uint32_t trail_surrogate = CUTF_MASK16(*start);
			++start;
			cp = (cp << 10) + trail_surrogate + CUTF_SURROGATE_OFFSET;
		}
		it = cutf_append(cp, it);
	}
	return it - out;
}

size_t cutf_8to16(uint8_t* start, uint8_t* end, uint16_t* out)
{
	uint16_t* it = out;
	while(start < end) {
		uint32_t cp = cutf_next(&start);
		if(cp > 0xffff) { //make a surrogate pair
			*(it++) = (uint16_t)((cp >> 10)   + CUTF_LEAD_OFFSET);
			*(it++) = (uint16_t)((cp & 0x3ff) + CUTF_TRAIL_SURROGATE_MIN);
		} else {
			*(it++) = (uint16_t)(cp);
		}
	}
	return it - out;
}

size_t cutf_32to8(uint32_t* start, uint32_t* end, uint8_t* out)
{
	uint8_t* it = out;
	for(; start != end; ++start) {
		it = cutf_append(*start, it);
	}
	return it - out;
}

size_t cutf_8to32(uint8_t* start, uint8_t* end, uint32_t* out)
{
	uint32_t* it = out;
	for(; start < end; ++it) {
		*it = cutf_next(&start);
	}
	return it - out;
}

size_t cutf_replace_invalid(uint8_t* start, uint8_t* end, uint8_t* out, size_t limit, uint32_t replacement)
{
	uint8_t* it = out;
	limit -= cutf_codepoint_length(replacement);
	while(start != end && (size_t)(it - out) <= limit ) {
		uint8_t* sequence = start;
		enum cutf_error err_code = cutf_validate_next(&start, end);
		switch (err_code) {
			case CUTF_OK :
				for(; sequence != start; ++sequence)
					*it++ = *sequence;
				break;
			case CUTF_INVALID_LEAD:
				it = cutf_append(replacement, it);
				++start;
				break;
			case CUTF_TRUNCATED_SEQUENCE:
			case CUTF_INCOMPLETE_SEQUENCE:
			case CUTF_OVERLONG_SEQUENCE:
			case CUTF_INVALID_CODE_POINT:
				it = cutf_append(replacement, it);
				++start;
				// just one replacement mark for the sequence
				while(start != end && CUTF_IS_TRAIL(*start))
					++start;
				break;
		}
	}
	return it - out;
}

size_t cutf_default_replace_invalid(uint8_t* start, uint8_t* end, uint8_t* out, size_t limit)
{
	const uint32_t replacement_marker = CUTF_MASK16(0xfffd);
	return cutf_replace_invalid(start, end, out, limit, replacement_marker);
}
#endif // CUTF_IMPLEMENTATION

#endif // CUTF_H
