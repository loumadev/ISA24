/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <sys/types.h>

#include "stream/Readable.cpp"

#define BUFFER_SIZE 2048

class BufferedReader {
private:
	Readable *stream;
	char *buffer;
	size_t capacity;
	size_t size;
	size_t position;
public:
	BufferedReader(Readable *stream, size_t capacity = BUFFER_SIZE) {
		this->stream = stream;
		this->capacity = capacity;
		this->buffer = new char[capacity];
		this->size = 0;
		this->position = 0;
	}

	~BufferedReader() {
		delete[] this->buffer;
	}

	int readByte() {
		// Reading past the end of the buffer
		if(this->position >= this->size) {
			// Load more data from the stream and reset the position
			this->size = this->stream->read(this->buffer, this->capacity);
			this->position = 0;
		}

		// EOF reached
		if(this->size == 0) return -1;

		return this->buffer[this->position++];
	}

	size_t readBytes(char *buffer, size_t size) {
		size_t bytesRead = 0;

		while(bytesRead < size) {
			int byte = this->readByte();

			// EOF reached
			if(byte == -1) break;

			buffer[bytesRead++] = byte;
		}

		return bytesRead;
	}

	size_t skipBytes(size_t size) {
		size_t bytesSkipped = 0;

		while(bytesSkipped < size) {
			int byte = this->readByte();

			// EOF reached
			if(byte == -1) break;

			bytesSkipped++;
		}

		return bytesSkipped;
	}

	ssize_t readLine(char *buffer, size_t size) {
		size_t bytesRead = 0;

		while(bytesRead < size) {
			int byte = this->readByte();

			// EOF reached
			if(byte == -1) {
				if(bytesRead == 0) return -1;
				break;
			}

			if(byte == '\r') {
				// Try to find the LF
				int nextByte = this->readByte();
				if(nextByte == -1) break;
				if(nextByte == '\n') break;

				// If the next byte is not LF, put it back to the buffer
				this->position--;
				break;
			}

			if(byte == '\n') break;

			buffer[bytesRead++] = byte;
		}

		return bytesRead;
	}

	ssize_t skipLine() {
		size_t bytesSkipped = 0;

		while(true) {
			int byte = this->readByte();

			// EOF reached
			if(byte == -1) {
				if(bytesSkipped == 0) return -1;
				break;
			}

			bytesSkipped++;

			if(byte == '\r') {
				// Try to find the LF
				int nextByte = this->readByte();
				if(nextByte == -1) break;
				if(nextByte == '\n') break;

				// If the next byte is not LF, put it back to the buffer
				this->position--;
				break;
			}

			if(byte == '\n') break;
		}

		return bytesSkipped;
	}
};
