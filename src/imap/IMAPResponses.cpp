/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <regex>

#include <iostream>

#include "stream/BufferedReader.cpp"
#include "stream/ReadableString.cpp"

#include "inspector.h"

class SafeStringView {
public:
	std::string *data;
	size_t start;
	size_t end;

public:
	SafeStringView() : data(nullptr), start(0), end(0) {}

	SafeStringView(std::string *data, size_t start, size_t end) {
		this->data = data;
		this->start = start;
		this->end = end;

		if(this->start > this->end) {
			throw std::runtime_error("Invalid SafeStringView bounds");
		}

		if(this->end > this->data->length()) {
			throw std::runtime_error("Invalid SafeStringView bounds");
		}
	}

	size_t length() {
		return this->end - this->start;
	}

	std::string_view toView() {
		if(!this->data) return std::string_view("");
		return std::string_view(this->data->c_str() + this->start, this->end - this->start);
	}

	std::string toString() {
		if(!this->data) return std::string("");
		return std::string(this->data->c_str() + this->start, this->end - this->start);
	}
};

class IMAPResponseBodyLine {
public:
	enum class Type {
		TAGGED,
		UNTAGGED,
		CONTINUATION
	};

public:
	Type type;
	SafeStringView content;
	std::optional<SafeStringView> octetStream;

public:
	IMAPResponseBodyLine(SafeStringView content) {
		this->content = content;
		this->resolveType();
	}

	IMAPResponseBodyLine(SafeStringView content, SafeStringView octetStream) {
		this->content = content;
		this->octetStream = octetStream;
		this->resolveType();
	}

private:
	void resolveType() {
		std::string_view content = this->content.toView();
		this->type = content.starts_with('*') ? Type::UNTAGGED : content.starts_with('+') ? Type::CONTINUATION : Type::TAGGED;
	}
};

class IMAPRawResponse {
public:
	enum class Status {
		OK,
		BAD,
		NO,
		PREAUTH,
		BYE
	};

public:
	uint32_t tag;
	Status status;
	SafeStringView message;
	std::vector<IMAPResponseBodyLine> bodyLines;
	std::string *raw;

public:
	IMAPRawResponse() {
		this->tag = 0;
		this->status = Status::OK;
		this->raw = new std::string();
		this->raw->reserve(2048);
	}

	~IMAPRawResponse() {
		delete this->raw;
	}

	bool isOk() {
		return this->status == Status::OK ||
		       this->status == Status::PREAUTH ||
		       this->status == Status::BYE;
	}
};

class IMAPResponse {
public:
	IMAPRawResponse *response;

public:
	IMAPResponse(IMAPRawResponse *response) {
		this->response = response;
	}

	bool isOk() {
		return this->response->isOk();
	}
};

class IMAPLoginResponse : public IMAPResponse {};
class IMAPLogoutResponse : public IMAPResponse {};

class IMAPSelectResponse : public IMAPResponse {
public:
	uint32_t exists;
	uint32_t recent;

public:
	IMAPSelectResponse(IMAPRawResponse *response) : IMAPResponse(response) {
		this->exists = 0;
		this->recent = 0;

		if(this->response->status != IMAPRawResponse::Status::OK) return;
		this->parseBody();
	}

private:
	void parseBody() {
		// 1 = number of existing messages
		std::regex r_exists("^\\*\\s+([0-9]+)\\s+EXISTS");
		// 1 = number of recent messages
		std::regex r_recent("^\\*\\s+([0-9]+)\\s+RECENT");

		// Loop through the body lines
		for(IMAPResponseBodyLine &line : this->response->bodyLines) {
			std::string_view content = line.content.toView();

			// Match the number of existing messages
			std::cmatch m_exists;
			if(std::regex_search(content.begin(), content.end(), m_exists, r_exists)) {
				this->exists = std::stoul(m_exists[1]);
				continue;
			}

			// Match the number of recent messages
			std::cmatch m_recent;
			if(std::regex_search(content.begin(), content.end(), m_recent, r_recent)) {
				this->recent = std::stoul(m_recent[1]);
				continue;
			}
		}
	}
};

class IMAPSearchResponse : public IMAPResponse {
public:
	std::vector<uint32_t> messageNumbers;

public:
	IMAPSearchResponse(IMAPRawResponse *response) : IMAPResponse(response) {
		if(this->response->status != IMAPRawResponse::Status::OK) return;
		this->parseBody();
	}

private:
	void parseBody() {
		// 1 = message number list
		std::regex r_numberList("^\\*\\s+SEARCH((?:\\s+[0-9]+)+)");
		// 0 = message number
		std::regex r_number("[0-9]+");

		// Loop through the body lines
		for(IMAPResponseBodyLine &line : this->response->bodyLines) {
			std::string_view content = line.content.toView();

			// Match the message number list
			std::cmatch m_numberList;
			if(!std::regex_search(content.begin(), content.end(), m_numberList, r_numberList)) {
				continue;
			}

			// Create a string view of the message number list
			size_t numberListStart = m_numberList.position(1);
			size_t numberListEnd = numberListStart + m_numberList.length(1);
			std::string_view numberList = content.substr(numberListStart, numberListEnd - numberListStart);

			// Match the message numbers
			std::cmatch m_number;
			std::string_view::const_iterator start = numberList.begin();
			std::string_view::const_iterator end = numberList.end();
			while(std::regex_search(start, end, m_number, r_number)) {
				// Add the message number to the list
				this->messageNumbers.push_back(std::stoul(m_number[0]));

				// Move the start iterator
				start = m_number[0].second;
			}

			break;
		}
	}
};

class IMAPMessage {
public:
	uint32_t id;
	SafeStringView content;
	std::map<std::string, std::string> headers;

public:
	IMAPMessage() {
		this->id = 0;
	}

	void parseHeaders() {
		// Need to create a variable, cannot be passed directly as argument (idk why)
		std::string str = this->content.toString();

		ReadableString stream(str.c_str(), this->content.length());
		BufferedReader reader(&stream);

		std::string lastKey;

		char buffer[1024];
		while(true) {
			ssize_t bytesRead = reader.readLine(buffer, sizeof(buffer));
			if(bytesRead == -1) break; // EOF
			if(bytesRead == 0) break; // End of headers

			// Create a string object
			std::string line(buffer, bytesRead);

			// Get the position of first non whitespace character
			size_t firstNonWhitespace = line.find_first_not_of(" \t");

			// Folded header
			if(firstNonWhitespace != 0) {
				// Append the line to the last key
				this->headers[lastKey] += line.substr(firstNonWhitespace);
				continue;
			}

			// Split the line into key and value
			const char delim[] = ": ";
			size_t pos = line.find(delim);
			if(pos == std::string::npos) continue;

			// Get the key and value
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + sizeof(delim) - 1);

			// Convert the key to lowercase
			std::transform(key.begin(), key.end(), key.begin(), ::tolower);

			// Add the header to the map
			this->headers[key] = value;
			lastKey = key;
		}

		// Debug print all headers
		// for(auto &header : this->headers) {
		// 	print_debug("Header: '%s' = '%s'", header.first.c_str(), header.second.c_str());
		// }
	}
};

class IMAPFetchResponse : public IMAPResponse {
public:
	std::vector<IMAPMessage> messages;

public:
	IMAPFetchResponse(IMAPRawResponse *response) : IMAPResponse(response) {
		if(this->response->status != IMAPRawResponse::Status::OK) return;
		this->parseBody();
	}

private:
	void parseBody() {
		// 1 = message ID
		std::regex r_id("^\\*\\s+([0-9]+)\\s+FETCH");

		// Loop through the body lines
		for(IMAPResponseBodyLine &line : this->response->bodyLines) {
			std::string_view content = line.content.toView();

			// Match the message ID
			std::cmatch m_id;
			if(!std::regex_search(content.begin(), content.end(), m_id, r_id)) {
				continue;
			}

			// Create a message object
			IMAPMessage message;
			message.id = std::stoul(m_id[1]);

			if(line.octetStream.has_value()) {
				message.content = line.octetStream.value();
				message.parseHeaders();
			} else {
				message.content = SafeStringView();
			}

			// Add the message to the list
			this->messages.push_back(message);
		}
	}
};
