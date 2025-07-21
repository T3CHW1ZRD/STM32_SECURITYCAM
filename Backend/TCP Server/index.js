const net = require('net');
const crypto = require('crypto'); // using crypto to encrypt and decrypt
const fs = require('fs');
const algorithm = 'aes-128-ctr';
const key = "something";
const server = net.createServer((socket) => {
    console.log('Connection established')
    socket.on('data',  buffer =>{
        console.log('Received bytes', buffer.length);
        const iv = buffer.subarray(0, 16); // first 16 bytes are the initialisation vector
        const remaining = buffer.subarray(16);//the encrypted struct

        //https://nodejs.org/api/crypto.html#class-decipheriv
        const decipher = crypto.createDecipheriv(algorithm, key, iv);
        let decrypted = decipher.update(remaining);
        decrypted += decipher.final();
        console.log(decrypted);
        

        // To be removed later and added to HTTP Server
        //parsing the decrypted data
        const command_id = decrypted.readUInt8(0);

        const command_arg = decrypted.readUInt16(1);
        const data_len = decrypted.readUInt32(3);
        const padding_len = decrypted.readUInt8(7);
        const timestamp = decrypted.readUInt32(8);
        const payload = decrypted.subarray(12, 12 + data_len); 

        const filename = `img_${Date.now()}.jpg`;


        fs.writeFile('Image', filename, payload, (err)=>{
            if(err){
                console.error('Image saving failed', err);
            }
            else{
                console.log('Received Image decrypted and Saved');
            }
        })
        socket.write('Receiving successful');
    });
    socket.on('end', () =>{
        console.log('Disconnected');
    });
});

const PORT = 5001;
server.listen(PORT,() =>{
    console.log(`TCP Server running on port ${PORT}`);
});