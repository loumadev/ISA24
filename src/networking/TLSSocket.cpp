/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "networking/Socket.cpp"
#include "networking/TCPSocket.cpp"
#include "networking/AddressInfo.cpp"
#include "inspector.h"

class TLSSocket : public Socket {
public:
	SSL_CTX *ctx;
	SSL *ssl;
	TCPSocket *tcpSocket;

public:
	TLSSocket() {
		// Set up the SSL library
		SSL_library_init();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();

		// Create a new SSL context
		this->ctx = SSL_CTX_new(SSLv23_client_method());
		if(this->ctx == NULL) {
			print_debug("Error: Failed to create SSL context");
			throw std::runtime_error("Failed to create SSL context");
		}

		// Create a new TCP socket
		this->tcpSocket = new TCPSocket();
	}

	void connect(const char *hostname, int port) {
		// Connect the TCP socket
		this->tcpSocket->connect(hostname, port);

		// Create a new SSL structure
		this->ssl = SSL_new(this->ctx);
		if(this->ssl == NULL) {
			print_debug("Error: Failed to allocate SSL structure");
			throw std::runtime_error("Failed to allocate SSL structure");
		}

		// Set the file descriptor for the SSL structure
		SSL_set_fd(this->ssl, this->tcpSocket->fd);

		// Initiate the SSL handshake
		if(SSL_connect(this->ssl) != 1) {
			print_debug("Error: Failed to establish SSL connection");
			throw std::runtime_error("Failed to establish SSL connection");
		}

		print_debug("Connected to %s:%d", hostname, port);
	}

	void write(const char *buffer, size_t size) {
		if(SSL_write(this->ssl, buffer, size) == -1) {
			print_debug("Error: Failed to send data (%d)", errno);
			// throw std::runtime_error(std::format("Failed to send data ({})", errno));
			throw std::runtime_error("Failed to send data (" + std::to_string(errno) + ")");
		}
	}

	size_t read(char *buffer, size_t size) {
		ssize_t bytes = SSL_read(this->ssl, buffer, size);

		if(bytes == -1) {
			print_debug("Error: Failed to receive data (%d)", errno);
			// throw std::runtime_error(std::format("Failed to receive data ({})", errno));
			throw std::runtime_error("Failed to receive data (" + std::to_string(errno) + ")");
		}

		return bytes;
	}

	void close() {
		SSL_shutdown(this->ssl);
		SSL_free(this->ssl);
		SSL_CTX_free(this->ctx);
		this->tcpSocket->close();
	}

	void setCertificateFile(const char *certFile) {
		if(SSL_CTX_load_verify_file(this->ctx, certFile) != 1) {
			print_debug("Error: Failed to load certificate file");
			throw std::runtime_error("Failed to load certificate file");
		}
	}

	void setCertificateDirectory(const char *certDir) {
		if(SSL_CTX_load_verify_dir(this->ctx, certDir) != 1) {
			print_debug("Error: Failed to load certificate directory");
			std::runtime_error("Failed to load certificate directory");
		}
	}
};