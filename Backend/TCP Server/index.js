const net = require('net');
const crypto = require('crypto'); // using crypto to encrypt and decrypt

const algorithm = 'aes-128-ctr';
const key = "something";
const server = net.createServer((socket) => {
    console.log('Connection established')
    socket.on('data',  buffer =>{
        console.log('Received bytes', buffer.length);
        const iv = buffer.subarray(0, 16); // first 16 bytes are the initialisation vector
        const remaining = buffer.subarray(16);//the encrypted struct

        const decipher = crypto.createDecipheriv(algorithm, key, iv);
        
        
        socket.write('Receiving successful');
    });
    socket.on('end', () =>{
        console.log('Disconnected');
    });
});

const PORT = 5000;
server.listen(PORT,() =>{
    console.log(`TCP Server running on port ${PORT}`);
});