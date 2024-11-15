const net = require("net");
const tls = require("tls");

const HOST = "imap.example.com";
const PORT = 993; // Standard port for IMAP over SSL/TLS

const username = "user@example.com";
const password = "password";

let buffer = "";
let tag = 0;

function getTag() {
	return `a${++tag}`;
}

function sendCommand(socket, command) {
	const fullCommand = `${getTag()} ${command}\r\n`;
	console.log("Client:", fullCommand.trim());
	socket.write(fullCommand);
}

function connectIMAP() {
	const socket = tls.connect(PORT, HOST, {rejectUnauthorized: false}, () => {
		console.log("Connected to IMAP server");
	});

	let state = "INIT";

	socket.on("data", (data) => {
		buffer += data.toString();
		let lines = buffer.split("\r\n");

		for(let i = 0; i < lines.length - 1; i++) {
			const line = lines[i];
			console.log("Server:", line);

			switch(state) {
				case "INIT":
					if(line.startsWith("* OK")) {
						sendCommand(socket, `LOGIN ${username} ${password}`);
						state = "LOGIN";
					}
					break;
				case "LOGIN":
					if(line.includes("OK")) {
						sendCommand(socket, 'LIST "" "*"');
						state = "LIST_MAILBOXES";
					}
					break;
				case "LIST_MAILBOXES":
					if(line.startsWith("* LIST")) {
						// Extract mailbox name
						const match = line.match(/"\/" "(.+)"/);
						if(match) {
							console.log("Mailbox found:", match[1]);
						}
					} else if(line.includes("OK LIST")) {
						sendCommand(socket, "SELECT INBOX");
						state = "SELECT_INBOX";
					}
					break;
				case "SELECT_INBOX":
					if(line.includes("OK [READ-WRITE] SELECT")) {
						sendCommand(socket, "FETCH 1:* (FLAGS BODY[HEADER.FIELDS (SUBJECT FROM)])");
						state = "FETCH_EMAILS";
					}
					break;
				case "FETCH_EMAILS":
					if(line.startsWith("*")) {
						if(line.includes("BODY[HEADER.FIELDS (SUBJECT FROM)]")) {
							console.log("Email headers:", line);
						}
					} else if(line.includes("OK FETCH")) {
						sendCommand(socket, "LOGOUT");
						state = "LOGOUT";
					}
					break;
				case "LOGOUT":
					if(line.includes("OK LOGOUT")) {
						console.log("Logged out successfully");
						socket.end();
					}
					break;
			}
		}

		buffer = lines[lines.length - 1];
	});

	socket.on("close", () => {
		console.log("Connection closed");
	});

	socket.on("error", (err) => {
		console.error("Socket error:", err);
	});
}

connectIMAP();