/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#include <string>
#include <regex>

#include "inspector.h"

#include "app/ConfigFileReader.cpp"
#include "app/Utils.cpp"
#include "stream/FileStream.cpp"
#include "networking/Socket.cpp"
#include "networking/TCPSocket.cpp"
#include "networking/TLSSocket.cpp"
#include "imap/IMAPClient.cpp"

// #define PRINT_ERROR(__name, __template, ...) (std::cerr << RED BOLD << __name << ": " RST << std::format(__template, ## __VA_ARGS__) << std::endl)
#define PRINT_ERROR(__name, __string) (std::cerr << RED BOLD << __name << ": " RST << __string << std::endl)
#define ARGUMENT_ERROR(__string) PRINT_ERROR("Argument Error", __string)
#define CLIENT_ERROR(__string) PRINT_ERROR("Client Error", __string)

void printHelp(const char *program, bool simple = true) {
	if(simple) {
		std::cout << "Usage: " << program << " server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir" << std::endl;
		return;
	}

	std::cout << "Usage: " << program << " server [-p port] [-T [-c certfile] [-C certaddr]] [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -p <port>        Port number (default: 143 for IMAP, 993 for IMAPS)" << std::endl;
	std::cout << "  -T               Use TLS" << std::endl;
	std::cout << "  -c <file>        Certificate file path" << std::endl;
	std::cout << "  -C <dir>         Certificate directory path" << std::endl;
	std::cout << "  -n               Fetch only recent messages" << std::endl;
	std::cout << "  -h               Fetch only headers" << std::endl;
	std::cout << "  -a <file>        Auth file path" << std::endl;
	std::cout << "  -b <mailbox>     Mailbox name (default: INBOX)" << std::endl;
	std::cout << "  -o <dir>         Output directory path" << std::endl;
}

class ClientSettings {
public:
	const char *host = nullptr;
	uint32_t port = 0;

	bool tls = false;
	const char *certFile = nullptr;
	const char *certDir = "/etc/ssl/certs";

	const char *mailbox = "INBOX";
	bool headersOnly = false;
	bool recentOnly = false;

	const char *authFile = nullptr;
	const char *outDir = nullptr;

public:
	bool validate() {
		if(!this->host) return ARGUMENT_ERROR("Missing host"), false;
		if(this->port == 0) this->port = this->tls ? 993 : 143;
		// if(this->port < 1 || this->port > 65535) return ARGUMENT_ERROR("Invalid port '{}'", this->port), false;
		if(this->port < 1 || this->port > 65535) return ARGUMENT_ERROR("Invalid port '" + std::to_string(this->port) + "'"), false;
		if(!this->authFile) return ARGUMENT_ERROR("Missing auth file"), false;
		if(!this->outDir) return ARGUMENT_ERROR("Missing output directory"), false;
		return true;
	}

	std::string formatDownloadCount(size_t count) {
		const char *messageCount = count == 1 ? "message" : "messages";
		const char *headerCount = count == 1 ? "message header" : "message headers";
		const char *itemType = this->headersOnly ? headerCount : messageCount;
		const char *newFlag = this->recentOnly ? "new " : "";

		// if(count != 0) return std::format("Downloaded {} {}{} from mailbox {}.", count, newFlag, itemType, this->mailbox);
		// else return std::format("No {}{} to download from mailbox {}.", newFlag, itemType, this->mailbox);

		if(count != 0) return "Downloaded " + std::to_string(count) + " " + newFlag + itemType + " from mailbox " + this->mailbox + ".";
		else return "No " + std::string(newFlag) + "" + itemType + " to download from mailbox " + this->mailbox + ".";
	}
};

void runClient(ClientSettings *settings) {
	#define CHECK_RESPONSE(__response) if(!__response->isOk()) {throw std::runtime_error("Request failed (" + __response->response->message.toString() + ")");}

	// Parse the auth file
	ConfigFileReader config(settings->authFile);
	config.parse();

	const char *username = config.entries.contains("username") ? config.entries["username"].c_str() : nullptr;
	const char *password = config.entries.contains("password") ? config.entries["password"].c_str() : nullptr;

	// Validate the auth file
	if(!username || !password) {
		throw std::runtime_error("Missing username or password entries in the auth file");
	}

	// Create appropriate socket
	Socket *socket = settings->tls ? (Socket*)new TLSSocket() : (Socket*)new TCPSocket();

	if(settings->tls) {
		if(settings->certFile) ((TLSSocket*)socket)->setCertificateFile(settings->certFile);
		if(settings->certDir) ((TLSSocket*)socket)->setCertificateDirectory(settings->certDir);
	}

	// Connect to the server
	socket->connect(settings->host, settings->port);

	// Create the IMAP client
	IMAPClient client(socket);

	// Login to the server
	IMAPLoginResponse *loginResponse = client.login(username, password);
	CHECK_RESPONSE(loginResponse);

	// Select the mailbox
	IMAPSelectResponse *selectResponse = client.selectMailbox(settings->mailbox);
	CHECK_RESPONSE(selectResponse);

	// Prepare fetch query
	IMAPClient::FetchQuery query;

	// Only fetch recent messages
	if(settings->recentOnly) {
		// Search for recent messages first
		IMAPSearchResponse *searchResponse = client.search("RECENT");
		CHECK_RESPONSE(searchResponse);

		query.addMessageList(searchResponse->messageNumbers);
	} else {
		// Fetch all messages
		query.addMessageRange(1, -1);
	}

	// Fetch only headers
	if(settings->headersOnly) {
		query.peekOnly();
	} else {
		query.addBody();
	}

	// Only perform the fetch if there are messages to fetch
	if(!query.isEmpty()) {
		IMAPFetchResponse *fetchResponse = client.fetch(&query);
		CHECK_RESPONSE(fetchResponse);

		// Save the fetched messages to the output directory
		for(IMAPMessage &message : fetchResponse->messages) {
			// Get the message ID
			std::string messageId = message.headers.contains("message-id") ?
				message.headers["message-id"] :
				// std::format("__unknown_message_{}__", message.id);
				"__unknown_message_" + std::to_string(message.id) + "__";

			// Hash the message ID
			uint8_t hash[16];
			Utils::hashString(hash, messageId.c_str(), messageId.length());
			std::string hex = Utils::stringifyBuffer(hash, 16);

			// Construct the file path
			print_debug("Saving message %s to %s", messageId.c_str(), hex.c_str());
			// std::string filepath = std::format("{}/{}.eml", settings->outDir, hex);
			std::string filepath = std::string(settings->outDir) + "/" + hex + ".eml";

			// Write the message contents to the file
			FileStream fileStream(filepath.c_str(), "w");
			fileStream.write(message.content.toString().c_str(), message.content.length());
		}

		// Print the number of fetched messages
		std::cout << settings->formatDownloadCount(fetchResponse->messages.size()) << std::endl;

		delete fetchResponse;
	} else {
		std::cout << settings->formatDownloadCount(0) << std::endl;
	}

	// Logout from the server
	IMAPLogoutResponse *logoutResponse = client.logout();
	CHECK_RESPONSE(logoutResponse);

	// Close the socket
	socket->close();

	// Cleanup
	delete logoutResponse;
	delete selectResponse;
	delete loginResponse;
	delete socket;
}

int main(const int argc, const char **argv) {
	(void)argc;

	const char *program = *argv++;
	(void)program;

	// Prepare the client settings
	ClientSettings settings;

	// The first argument should be the host
	settings.host = *argv++;
	if(!settings.host) {
		settings.validate(); // This should print the error message right away
		printHelp(program);
		return 1;
	}

	// Parse the rest of the arguments
	#define CASE(__switch) if(strcmp(arg, __switch) == 0)
	#define FETCH(__var, __msg) const char *__var = *argv++; if(!__var) {ARGUMENT_ERROR(__msg); return 1;}

	const char *arg = nullptr;
	while((arg = *argv++)) {
		CASE("-p") {
			FETCH(p, "Missing port value");
			try {
				settings.port = std::stoul(p);
			} catch(...) {
				// ARGUMENT_ERROR("Invalid port value '{}'", p);
				ARGUMENT_ERROR("Invalid port value '" + std::string(p) + "'");
				return 1;
			}
		}
		CASE("-T") {
			settings.tls = true;
		}
		CASE("-c") {
			FETCH(f, "Missing certificate file path");
			settings.certFile = f;
		}
		CASE("-C") {
			FETCH(d, "Missing certificate directory path");
			settings.certDir = d;
		}
		CASE("-n") {
			settings.recentOnly = true;
		}
		CASE("-h") {
			settings.headersOnly = true;
		}
		CASE("-a") {
			FETCH(f, "Missing auth file path");
			settings.authFile = f;
		}
		CASE("-b") {
			FETCH(b, "Missing mailbox name");
			settings.mailbox = b;
		}
		CASE("-o") {
			FETCH(d, "Missing output directory path");
			settings.outDir = d;
		}
	}

	#undef CASE
	#undef FETCH

	// Validate the input arguments
	if(!settings.validate()) return printHelp(program), 1;

	// Run the client
	try {
		runClient(&settings);
	} catch(const std::exception &e) {
		// CLIENT_ERROR("{}", e.what());
		CLIENT_ERROR(e.what());
		return 1;
	}

	return 0;
}
