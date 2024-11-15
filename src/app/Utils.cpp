/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <stdint.h>
#include <string>

class Utils {
public:
	static void hashString(uint8_t hash[16], const char *str, const size_t length) {
		// Calculate rounds inversely proportional to the length of the string
		const int hashSize = 16;
		const int maxRounds = 8;
		const int rounds = std::max(1, maxRounds - static_cast<int>(length / 4));

		// Initialize the hash with a prime-based scheme for better entropy
		uint8_t seed = 0xA5; // Arbitrary seed to mix things up
		for(int i = 0; i < hashSize; ++i) {
			hash[i] = seed + i * 31;
		}

		// Mix input string into the hash
		for(size_t i = 0; i < length; ++i) {
			hash[i % hashSize] ^= str[i] + (i * seed);
		}

		// Perform the hashing rounds
		for(int r = 0; r < rounds; ++r) {
			for(size_t i = 0; i < hashSize; ++i) {
				// Use both neighbors to introduce more chaos
				uint8_t prev = hash[(i - 1 + hashSize) % hashSize];
				uint8_t next = hash[(i + 1) % hashSize];

				hash[i] ^= (prev >> 1) ^ (next << 1) ^ (r + i);
				hash[i] += ((r + 1) * 31 + seed);
			}

			// Shift the seed every round for extra variability
			seed = (seed * 33) ^ r;
		}
	}

	static std::string stringifyBuffer(const uint8_t *buffer, const size_t length) {
		std::string str;
		str.reserve(length * 2);

		for(size_t i = 0; i < length; ++i) {
			str += "0123456789abcdef"[buffer[i] >> 4];
			str += "0123456789abcdef"[buffer[i] & 0x0F];
		}

		return str;
	}
};