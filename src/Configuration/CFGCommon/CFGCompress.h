#ifndef CFGOBJECT_H
#define CFGOBJECT_H

#include <string>
#include <vector>

class CFG_COMPRESS {
public:
  
  static void compress
  (
    const uint8_t * input,
    const size_t input_size,
    std::vector<uint8_t>& output,
    size_t * header_size = nullptr,
    const bool support_followup_byte = false,
		const uint8_t debug = 0
  );
  static void decompress
  (
    const uint8_t * input,
    const size_t input_size,
    std::vector<uint8_t>& output,
    const uint8_t debug = 0
  );

private:

	static void analyze_repeat
	(
		const uint8_t * input,
    const size_t input_size,
		size_t index,
		size_t& length,
		size_t& repeat,
		const uint8_t debug
	);
	static size_t analyze_chunk_pattern
	(
		const uint8_t * input,
    const size_t input_size,
		size_t index,
		size_t length
	);
	static void compress_data
	(
		std::vector<uint8_t>& output,
		uint8_t compress_byte,
		size_t length,
		int followup_byte,
		const bool debug,
		std::string space
	);
	static void compress_repeat_chunk
	(
		const uint8_t * input,
    const size_t input_size,
		std::vector<uint8_t>& output,
		size_t index,
		size_t chunk_pattern,
		size_t length,
		size_t repeat
	);
	static size_t compress_none_repeat
	(
		const uint8_t * input,
    const size_t input_size,
		std::vector<uint8_t>& output,
		size_t index,
		const uint8_t debug
	);
	static void pack_data
	(
		const uint8_t * input,
    const size_t input_size,
		std::vector<uint8_t>& output,
		size_t index,
		size_t length,
		const bool debug
	);
};

#endif
