/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <regex>

#include "imap/IMAPResponses.cpp"
#include "stream/BufferedReader.cpp"
#include "networking/Socket.cpp"
#include "inspector.h"


class IMAPParser {
public:
	Socket *socket;
	BufferedReader *reader;
	std::map<uint32_t, IMAPRawResponse*> responses;
	bool hasConsumedHello = false;

public:
	IMAPParser(Socket *socket) {
		this->socket = socket;
		this->reader = new BufferedReader(socket);
	}

	~IMAPParser() {
		delete this->reader;
	}

	IMAPRawResponse* waitForResponse(uint32_t tag) {
		// Check if the response is already cached
		IMAPRawResponse *response = this->responses[tag];
		if(response) {
			// Remove the response from the cache and return it
			this->responses.erase(tag);
			return response;
		}

		// Keep parsing the messages until the requested response is received
		while(true) {
			// Handle the next response
			IMAPRawResponse *response = this->handleResponse();

			// If the response is the requested one, return it
			if(response->tag == tag) return response;

			// Cache the response for later
			this->responses[response->tag] = response;
		}
	}

	// Keep track of all received messages in some kind of array cache.
	// When a response is requested, check if the response is already cached, if yes, remove it from the cache and return it.
	// If not, read the buffer and parse the contents to create a new response object. After that check for the response type (and tag prefferably),
	// if it matches the requested response, return it, otherwise cache it and read the next response.

	IMAPRawResponse* handleResponse() {
		if(!this->hasConsumedHello) {
			this->reader->skipLine();
			this->hasConsumedHello = true;
		}

		IMAPRawResponse *response = new IMAPRawResponse();

		// And now ladies and gentlemen, the most beautiful IMAP response parser you've ever seen :kappa:
		int32_t bodyLineOffset = -1;
		int32_t octetStreamLengthOffset = -1;
		int32_t octetStreamOffset = -1;
		int32_t footerOffset = -1;
		size_t octetStreamLength = 0;
		bool hasCR = false;

		int byte = this->reader->readByte();
		while(byte != -1) {
			*response->raw += (char)byte;

			if(bodyLineOffset != -1) {
				if(octetStreamLengthOffset != -1) {
					if(byte == '}') {
						octetStreamLength = std::stoul(std::string(response->raw->c_str() + octetStreamLengthOffset, response->raw->length() - octetStreamLengthOffset - 1));

						char crlf[2];
						this->reader->readBytes(crlf, 2);
						if(crlf[0] != '\r' || crlf[1] != '\n') {
							delete response;
							throw std::runtime_error("Invalid response format (missing CRLF)");
						}
						*response->raw += crlf;

						octetStreamLengthOffset = -1;
						octetStreamOffset = response->raw->length();

						char *buffer = new char[octetStreamLength];
						size_t bytesRead = this->reader->readBytes(buffer, octetStreamLength);
						if(bytesRead != octetStreamLength) {
							delete[] buffer;
							delete response;
							throw std::runtime_error("Unexpected EOF");
						}

						*response->raw += std::string_view(buffer, octetStreamLength);
						delete[] buffer;

						// Drain the stream until the ')' is reached
						// (Server might append CRLF to the end of the octet stream)
						char c;
						while((c = this->reader->readByte()) != ')') {
							*response->raw += c;
						}
						*response->raw += c;
					}
				} else {
					if(byte == '{') {
						if(octetStreamLength != 0) {
							print_debug("Warning: Multiple octet streams in a single response are not supported");
						}
						octetStreamLengthOffset = response->raw->length();
					} else if(byte == '\r') {
						hasCR = true;
					} else if(byte == '\n') {
						if(!hasCR) {
							delete response;
							throw std::runtime_error("Invalid response format (missing CR)");
						}

						response->bodyLines.push_back(
							octetStreamOffset != -1 ?
								IMAPResponseBodyLine(
									SafeStringView(response->raw, bodyLineOffset, response->raw->length() - 2),
									SafeStringView(response->raw, octetStreamOffset, octetStreamOffset + octetStreamLength)
								) :
								IMAPResponseBodyLine(
									SafeStringView(response->raw, bodyLineOffset, response->raw->length() - 2)
								)
						);

						octetStreamLength = 0;
						octetStreamOffset = -1;
						bodyLineOffset = -1;
						hasCR = false;
					}
				}
			} else if(footerOffset != -1) {
				if(byte == '\r') {
					hasCR = true;
				} else if(byte == '\n') {
					if(!hasCR) {
						delete response;
						throw std::runtime_error("Invalid response format (missing CR)");
					}

					hasCR = false;
					break;
				}
			} else if(byte == '+' || byte == '*') {
				bodyLineOffset = response->raw->length() - 1;
			} else if(footerOffset == -1) {
				footerOffset = response->raw->length() - 1;
			}

			byte = this->reader->readByte();
		}

		if(footerOffset == -1) {
			delete response;
			throw std::runtime_error("Unexpected EOF");
		}

		std::string_view footer(response->raw->c_str() + footerOffset, response->raw->length() - footerOffset - 2);

		// 1 = tag, 2 = status, 3 = message
		std::regex footerRegex("^([a-zA-Z][a-zA-Z0-9]{0,1000})\x20([^\x20]+)\x20(.*)$");

		// Match the footer and get the groups
		std::cmatch match;
		if(!std::regex_search(footer.begin(), footer.end(), match, footerRegex)) {
			delete response;
			throw std::runtime_error("Invalid response format (footer)");
		}

		// Set the tag
		std::string tag = std::string(match[1].first, match[1].length());
		response->tag = std::stoul(tag.substr(1));

		// Get the uppercased status
		std::string status = std::string(match[2].first, match[2].length());
		std::transform(status.begin(), status.end(), status.begin(), ::toupper);

		// Set the status
		if(status == "OK") {
			response->status = IMAPRawResponse::Status::OK;
		} else if(status == "BAD") {
			response->status = IMAPRawResponse::Status::BAD;
		} else if(status == "NO") {
			response->status = IMAPRawResponse::Status::NO;
		} else if(status == "PREAUTH") {
			response->status = IMAPRawResponse::Status::PREAUTH;
		} else if(status == "BYE") {
			response->status = IMAPRawResponse::Status::BYE;
		} else {
			delete response;
			// throw std::runtime_error(std::format("Invalid response format (unknown status '{}')", status));
			throw std::runtime_error("Invalid response format (unknown status '" + status + "')");
		}

		int messageOffset = footerOffset + match.position(3);

		// Set the message
		response->message = SafeStringView(response->raw, messageOffset, messageOffset + match.length(3));

		return response;
	}
};
