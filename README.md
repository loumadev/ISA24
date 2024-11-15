# IMAP Client with TLS support

This project is a simple IMAP client with TLS support. It is able to connect to an IMAP server, authenticate, and download the messages in the mailbox. The project is written in C++ and uses the OpenSSL library for TLS support.

* **Author:** Jaroslav Louma (xlouma00)
* **Date:** 2024-11-15

## Installation

The project can be compiled using the provided Makefile, as follows:

```bash
make
```

To run the program, use the following command:

```bash
./imapcl server [-p port] [-T [-c certfile] [-C certaddr]]
                [-n] [-h] -a auth_file [-b MAILBOX] -o out_dir
```

## File structure

* **NOTICE:** The file `manual.pdf` was build from the provided `manual.md` source file using a private software. This software is not available to the public in any way.

```tree
.
├── docs
│   ├── assets
│   │   └── class_diagram.svg
│   ├── manual.md
│   └── manual.pdf
├── include
│   ├── assertf.h
│   ├── colors.h
│   ├── inspector.h
│   └── overload.h
├── src
│   ├── app
│   │   ├── ConfigFileReader.cpp
│   │   └── Utils.cpp
│   ├── imap
│   │   ├── IMAPClient.cpp
│   │   ├── IMAPParser.cpp
│   │   └── IMAPResponses.cpp
│   ├── main.cpp
│   ├── networking
│   │   ├── AddressInfo.cpp
│   │   ├── Socket.cpp
│   │   ├── TCPSocket.cpp
│   │   └── TLSSocket.cpp
│   └── stream
│       ├── BufferedReader.cpp
│       ├── FileStream.cpp
│       ├── Readable.cpp
│       ├── ReadableString.cpp
│       └── Writable.cpp
├── README.md
└── Makefile
```