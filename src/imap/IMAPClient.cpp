/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

// #include <format>
#include <utility>
#include <variant>

#include "imap/IMAPParser.cpp"
#include "networking/Socket.cpp"
#include "inspector.h"

#define LF "\n"
#define CR "\r"
#define CRLF "\r\n"

// std::string command = std::format(__format, ## __VA_ARGS__);
// std::string formatted = std::format("a{} {}\r\n", this->tag, command);
#define SEND_COMMAND(__string) do { \
		this->tag++; \
		print_debug("Sending command: '%s'", std::string(__string).c_str()); \
		std::string formatted = "a" + std::to_string(this->tag) + " " + __string + "\r\n"; \
		this->socket->write(formatted.c_str(), formatted.size()); \
} while(0)


class IMAPClient {
public:
	Socket *socket;
	IMAPParser *parser;
	int tag = 0;

public:
	IMAPClient(Socket *socket) {
		this->socket = socket;
		this->parser = new IMAPParser(socket);
	}

	~IMAPClient() {
		delete this->parser;
	}

	IMAPLoginResponse* login(const char *username, const char *password) {
		// SEND_COMMAND("LOGIN \"{}\" \"{}\"", IMAPClient::escapeString(username), IMAPClient::escapeString(password));
		SEND_COMMAND("LOGIN \"" + IMAPClient::escapeString(username) + "\" \"" + IMAPClient::escapeString(password) + "\"");
		IMAPRawResponse *response = this->parser->waitForResponse(this->tag);

		if(!response->isOk()) {
			print_debug("Error: Failed to login (%s)", response->message.toString().data());
		}

		return new IMAPLoginResponse(response);
	}

	IMAPSelectResponse* selectMailbox(const char *mailbox) {
		// SEND_COMMAND("SELECT \"{}\"", IMAPClient::escapeString(mailbox));
		SEND_COMMAND("SELECT \"" + IMAPClient::escapeString(mailbox) + "\"");
		IMAPRawResponse *response = this->parser->waitForResponse(this->tag);

		if(!response->isOk()) {
			print_debug("Error: Failed to select mailbox (%s)", response->message.toString().data());
		}

		return new IMAPSelectResponse(response);
	}

	IMAPSearchResponse* search(const char *criteria) {
		// SEND_COMMAND("UID SEARCH {}", criteria);
		SEND_COMMAND("UID SEARCH " + std::string(criteria));
		IMAPRawResponse *response = this->parser->waitForResponse(this->tag);

		if(!response->isOk()) {
			print_debug("Error: Failed to search mailbox (%s)", response->message.toString().data());
		}

		return new IMAPSearchResponse(response);
	}

	class FetchQuery {
public:
		std::string messageQuery;
		bool body = false;
		bool peek = false;

public:
		void addMessage(int32_t id) {
			this->appendMessageItem(FetchQuery::stringifyMessageId(id));
		}

		void addMessageRange(uint32_t start, int32_t end) {
			// this->appendMessageItem(std::format("{}:{}", start, FetchQuery::stringifyMessageId(end)));
			this->appendMessageItem(std::to_string(start) + ":" + FetchQuery::stringifyMessageId(end));
		}

		void addMessageList(std::vector<uint32_t> ids) {
			for(uint32_t id : ids) {
				this->addMessage(id);
			}
		}

		void addBody() {
			this->body = true;
		}

		void peekOnly() {
			this->peek = true;
		}

		bool isEmpty() {
			return this->messageQuery.empty();
		}

		std::string build() {
			std::string query = this->messageQuery;

			if(query.empty()) {
				query = "1:*";
			}

			query += " BODY";

			if(this->peek) {
				query += ".PEEK";
			}

			if(this->body) {
				query += "[]";
			} else {
				query += "[HEADER]";
			}

			return query;
		}

private:
		void appendMessageItem(std::string item) {
			if(!this->messageQuery.empty()) {
				this->messageQuery += ",";
			}

			this->messageQuery += item;
		}

		static std::string stringifyMessageId(int32_t id) {
			return id == -1 ? "*" : std::to_string(id);
		}
	};

	IMAPFetchResponse* fetch(FetchQuery *query) {
		// SEND_COMMAND("UID FETCH {}", query->build().c_str());
		SEND_COMMAND("UID FETCH " + query->build());
		IMAPRawResponse *response = this->parser->waitForResponse(this->tag);

		if(!response->isOk()) {
			print_debug("Error: Failed to fetch messages (%s)", response->message.toString().data());
		}

		return new IMAPFetchResponse(response);
	}

	IMAPLogoutResponse* logout() {
		SEND_COMMAND("LOGOUT");
		IMAPRawResponse *response = this->parser->waitForResponse(this->tag);

		if(!response->isOk()) {
			print_debug("Error: Failed to logout (%s)", response->message.toString().data());
		}

		return new IMAPLogoutResponse(response);
	}

private:
	static std::string escapeString(const char *str) {
		std::string escaped;
		escaped.reserve(strlen(str) * 2);

		for(const char *c = str; *c; c++) {
			if(*c == '"' || *c == '\\') {
				escaped += '\\';
			}

			escaped += *c;
		}

		return escaped;
	}
};

#undef SEND_COMMAND
