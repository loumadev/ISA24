//@ts-check

const fs = require("fs");
const {parser} = require("@lezer/cpp");

function collectClasses(filepath) {
	const source = fs.readFileSync(filepath, "utf8");
	const tree = parser.parse(source);

	// Genearate the UML Class diagram
	const classes = [];

	tree.iterate({
		enter(node) {
			if(node.type.name === "ClassSpecifier") {
				const range = source.slice(node.from, node.to);
				const result = parseClass(range);
				if(result) {
					classes.push(result);
					return false;
				} else {
					// console.log(range);
				}
			}
		},
		leave(node) {

		}
	});

	return classes;
}

function parseClass(source) {
	const classNode = {
		name: null,
		fields: /**@type {{access: any, type: any, name: any}[]}*/([]),
		methods: /**@type {{access: any, type: any, name: any}[]}*/([]),
		inheritance: /**@type {{access: string, baseClass: string}[]}*/([]),
		uses: /**@type {string[]}*/([]),
		isAbstract: false
	};

	// strip all comments and strings
	source = source
		.replace(/\/\/.*?$/gm, "")
		.replace(/\/\*.*?\*\//gs, "")
		.replace(/(["'])(?:[^"\\]|\\.)*\1/g, "");

	let [_, className, inheritanceList] = source.match(/^class\s+(.+?)\b(?:\s*:\s*(.*?))?\s*{/m) || [];
	if(!_) return null;

	classNode.name = className;

	if(inheritanceList) {
		classNode.inheritance = [...inheritanceList.matchAll(/\s*(virtual\s+)?(?:(public|private|protected)\s+)?(.+?)\s*(?:,|$)/gm)]
			.map(([_, isVirtual, access, baseClass]) => ({
				isVirtual: !!isVirtual,
				access: access || "private",
				baseClass
			}));
	}

	// Partion the code based on the access specifier
	const partitions = [...source.matchAll(/(private|protected|public):(.*?)(?=(?:private|protected|public):|$)/gs)]
		.map(([_, access, content]) => ({access, content}));

	// console.log(partitions);

	for(const partition of partitions) {
		// Parse fields
		const fields = [...partition.content.matchAll(/^(?:\t| {4})(static\s+)?(const\s+)?(cbfn\((?:[\w*&<>,\s]|::)*\)[\s*&]*|\w(?:[\w*&<>,\s]|::)+?)\s*(\w+?)\s*(?:;|=)/gm)]
			// const fields = [...partition.content.matchAll(/^(?:\t| {4})(static\s+)?(const\s+)?(\w(?:[\w*&<>,\s]|::)+?)\s*(\w+?)\s*(?:;|=)/gm)]
			.map(([_, isStatic, isConstant, type, name]) => ({
				access: partition.access,
				isStatic: !!isStatic,
				isConstant: !!isConstant,
				type: normalizeType(type),
				name: name.trim()
			}));

		// Parse methods
		const methods = [...partition.content.matchAll(/^(?:\t| {4})(?:(static)\s+|(virtual)\s+)?(cbfn\((?:[\w*&<>,\s]|::)*\)[\s*&]*|\w(?:[\w*&<>,\s]|::)+?\s+)?(~?\w+)\((.*?)\)\s*(?:{|(=)|;|:)/gm)]
			// const methods = [...partition.content.matchAll(/^(?:\t| {4})(?:(static)|(virtual)\s+)?(\w(?:[\w*&<>,\s]|::)+?\s+)?(\w+)\((.*?)\)\s*(?:{|=|;|:)/gm)]
			.map(([_, isStatic, isVirtual, type, name, parameters, isAbstract]) => ({
				access: partition.access,
				isConstructor: !type && name[0] !== "~",
				isDestructor: !type && name[0] === "~",
				isStatic: !!isStatic,
				isVirtual: !!isVirtual,
				isAbstract: !!isAbstract,
				type: normalizeType(type),
				name: name.trim(),
				parameters: [...parameters.matchAll(/(const\s+)?(cbfn\((?:[\w*&<>,\s]|::)*\)[\s*&]*|(?:\w(?:[\w*&<>,\s]|::)+?)(?:\s*(?:\*|&)\s*|\s+))(\w+)(?:.*?(?:,|$))/g)]
					.map(([_, isConstant, type, name]) => ({
						isConstant: !!isConstant,
						type: normalizeType(type),
						name: name.trim()
					}))
			}));

		classNode.fields.push(...fields);
		classNode.methods.push(...methods);

		if(!classNode.isAbstract && methods.some(m => m.isVirtual)) {
			classNode.isAbstract = true;
		}
	}

	// Find all using classes
	const usingClasses = [...source.matchAll(/(?<!::)\b([A-Z]\w+)/g)]
		.map(([_, name]) => name);

	classNode.uses = [...new Set(usingClasses)];

	return classNode;
}

function normalizeType(type) {
	if(!type) return null;
	return type
		.replace(/\s+/g, " ")
		.replace(/\s*([*&<>])\s*/g, "$1")
		.replace(/\s*(,)\s*/g, "$1 ")
		.trim();
}




const escapeHTML = (str) => str?.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

function formatAccess(access) {
	switch(access) {
		case "public": return "+";
		case "protected": return "#";
		case "private": return "-";
		default: return "~";
	}
}

function formatType(type) {
	if(!type) return "";
	return escapeHTML(type);
}

function generateDotFile(classes) {
	// Generate a DOT file
	let dot = `digraph G {
	graph [
		labelloc="t"
		fontname="Helvetica,Arial,sans-serif"
	]
	node [
		fontname="Helvetica,Arial,sans-serif"
		shape=record
		style=filled
		fillcolor=gray95
	]
	edge [fontname="Helvetica,Arial,sans-serif"]
`;

	// Create classes
	for(const cls of classes) {
		dot += `\t${cls.name} [\n`;
		dot += `\t\tshape=plain\n`;
		dot += `\t\tlabel=<<table border="0" cellborder="1" cellspacing="0" cellpadding="4">\n`;
		dot += `\t\t\t<tr> <td> <b>${cls.isAbstract ? "«abstract» " : ""}${cls.name}</b> </td> </tr>\n`;

		dot += `\t\t\t<tr> <td>\n`;
		dot += `\t\t\t\t<table border="0" cellborder="0" cellspacing="0" >\n`;
		for(const field of cls.fields) {
			dot += `\t\t\t\t\t<tr> <td align="left" >${formatAccess(field.access)} ${field.name}: ${formatType(field.type)}</td> </tr>\n`;
		}
		if(!cls.fields.length) dot += `\t\t\t\t\t<tr> <td align="left" > No fields </td> </tr>\n`;
		dot += `\t\t\t\t</table>\n`;
		dot += `\t\t\t</td> </tr>\n`;

		dot += `\t\t\t<tr> <td>\n`;
		dot += `\t\t\t\t<table border="0" cellborder="0" cellspacing="0" >\n`;
		for(const method of cls.methods) {
			dot += `\t\t\t\t\t<tr> <td align="left" >`;
			dot += `${method.isStatic ? "<u>" : ""}${method.isAbstract ? "<i>" : ""}`;
			if(method.isConstructor) {
				dot += `${formatAccess(method.access)} <i>constructor</i>(${method.parameters.map(param => `${param.name}: ${formatType(param.type)}`).join(", ")}): ${method.name}`;
			} else if(method.isDestructor) {
				dot += `${formatAccess(method.access)} <i>destructor</i>(${method.parameters.map(param => `${param.name}: ${formatType(param.type)}`).join(", ")}): void`;
			} else {
				dot += `${formatAccess(method.access)} ${method.name}(${method.parameters.map(param => `${param.name}: ${formatType(param.type)}`).join(", ")}): ${formatType(method.type)}`;
			}
			dot += `${method.isAbstract ? "</i>" : ""}${method.isStatic ? "</u>" : ""}`;
			dot += `</td> </tr>\n`;
			// if(isConstructor) {
			// 	dot += `\t\t\t\t\t<tr> <td align="left" >${formatAccess(method.access)} constructor(${method.parameters.map(param => `${param.name}: ${escapeHTML(param.type)}`).join(", ")}): ${method.name}</td> </tr>\n`;
			// } else {
			// 	dot += `\t\t\t\t\t<tr> <td align="left" >${formatAccess(method.access)} ${method.name}(${method.parameters.map(param => `${param.name}: ${escapeHTML(param.type)}`).join(", ")}): ${formatType(method.type)}</td> </tr>\n`;
			// }
		}
		if(!cls.methods.length) dot += `\t\t\t\t\t<tr> <td align="left" > No methods </td> </tr>\n`;
		dot += `\t\t\t\t</table>\n`;
		dot += `\t\t\t</td> </tr>\n`;

		dot += `\t\t\t\t</table>>\n`;
		dot += `\t]\n`;
	}

	// Create relations
	for(const cls of classes) {
		for(const inh of cls.inheritance) {
			// Change arrow type based on if the inherited class is abstract
			const inhCls = classes.find(c => c.name === inh.baseClass);
			if(!inhCls) {
				// console.log(cls);
				console.error(`Class ${inh.baseClass} not found`);
				continue;
			}

			const edgeStyle = inhCls.isAbstract ? "arrowtail=empty style=dashed" : "arrowtail=empty style=solid";

			dot += `\t${inh.baseClass} -> ${cls.name} [label="${inhCls.isAbstract && !cls.isAbstract ? "implements" : inhCls.isAbstract ? "extends" : "inherits"}" dir=back ${edgeStyle}]\n`;
		}

		for(const use of cls.uses) {
			if(cls.inheritance.some(inh => inh.baseClass === use)) continue;

			dot += `\t${cls.name} -> ${use} [arrowhead=vee style=dotted]\n`;
		}
	}

	dot += `}`;

	fs.writeFileSync("./ast.dot", dot);
}


function getFilesInDir(dir) {
	// Get all files in the directory and subdirectories
	const files = fs.readdirSync(dir);
	const result = [];
	for(const file of files) {
		const path = `${dir}/${file.replace(/\\/g, "/")}`;
		if(fs.statSync(path).isDirectory()) {
			result.push(...getFilesInDir(path));
		} else {
			result.push(path);
		}
	}
	return result;
}

(async function main() {
	// Get all files in the directory
	const files = getFilesInDir("./src")
		.filter(file => file.endsWith(".cpp"));

	const classes = [];
	for(const file of files) {
		classes.push(...collectClasses(file));
	}

	// Remove non-class types from using classes
	const classNames = new Set(classes.map(cls => cls.name));
	for(const cls of classes) {
		cls.uses = cls.uses.filter(name => classNames.has(name) && name !== cls.name);
	}

	// Print the classes
	console.log(classes.length);

	fs.writeFileSync("./ast.json", JSON.stringify(classes, null, "\t"));


	// Filter out classes
	// const includeClasses = new Set(["AddressInfo", "Events", "Socket", "TCPClient", "TCPServer", "UDPSocket"]);
	const includeClasses = new Set([
		// "EventLoop", "EventLoopEntity", "EventLoopTimer",
		// "Event", "EventDispatcher", "EventHandler", "OnceEventHandler",
		// "AddressInfo", "Socket", "TCPClient", "TCPServer", "UDPSocket",
		//"SocketEvent", "SocketErrorEvent", "UDPListeningEvent", "UDPMessageEvent", "TCPDataEvent", "TCPConnectEvent", "TCPCloseEvent",
		//"TCPConnectionEvent", "TCPListeningEvent"
		// "IPK24ProtocolImplementation", "IPK24ProtocolServerImplementation", "IPK24ProtocolClientImplementation",
		// "ProtocolMessageType",
		// "IdGenerator",
		// "ProtocolMessage",
		// "ConfirmProtocolMessage",
		// "ReplyProtocolMessage",
		// "AuthProtocolMessage",
		// "JoinProtocolMessage",
		// "MessageProtocolMessage",
		// "ErrorProtocolMessage",
		// "ByeProtocolMessage",
		// "ProtocolMessageParser", "ProtocolMessageSerializer",
		// "TCPProtocolMessageParser", "TCPProtocolMessageSerializer",
		// "UDPProtocolMessageParser", "UDPProtocolMessageSerializer",

		// "IPK24ProtocolUDPImplementation",
		// "IPK24ProtocolTCPClientImplementation", "IPK24ProtocolTCPServerImplementation",

		// "ChatApp", "CommandParser", "Command", "ReadLine"
		// "IPK24ChatServer"

		...classes.map(cls => cls.name)
	]);

	const filteredClasses = classes.filter(cls => includeClasses.has(cls.name));
	generateDotFile(filteredClasses);
})();
