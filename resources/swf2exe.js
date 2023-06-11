var fs = require("fs");

let footer = [0x56, 0x34, 0x12, 0xFA];

let projectorData = fs.readFileSync("windows/flashplayer_32_sa.exe");
let swfData = fs.readFileSync("hf.swf");

fs.writeFileSync("out.exe", projectorData);
fs.appendFileSync("out.exe", swfData);
fs.appendFileSync("out.exe", new Uint8Array(footer));
fs.appendFileSync("out.exe", Buffer.from( new Uint8Array(intToByteArray(swfData.length))));


function intToByteArray(int) {
	// we want to represent the input as a 4-bytes array
	var byteArray = [0, 0, 0, 0];

	for ( var index = 0; index < byteArray.length; index ++ ) {
		var byte = int & 0xff;
		byteArray [ index ] = byte;
		int = (int - byte) / 256 ;
	}

	return byteArray;
};