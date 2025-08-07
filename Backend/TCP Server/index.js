const net = require('net');
const crypto = require('crypto');
const fs = require('fs');
const axios = require('axios');
const Packet = require('./Packet');
require('dotenv').config();
const key = Buffer.from(process.env.KEY, 'hex');


const algorithm = 'aes-128-cdc';

function decryptPacket(iv, encrypted) {
    const decipher = crypto.createDecipheriv(algorithm, key, iv);
    const decrypted = Buffer.concat([decipher.update(encrypted), decipher.final()]);
    return decrypted;
}

function sendChallengeResponse(socket, payload) {
    socket.write(payload);
}
// Function to register the new device in the database using IP address as unique identifier. 
async function deviceRegistration(socket, clientIp){
    try{
        await axios.post('http://localhost:3000/device', {
            headers:{
                'Content-Type': 'application/json',
                'X-Client': 'clientIp',
            }
        });
        console.log('Device registered with Backend');
    }
    catch(err){
        console.log('Failed to forward image', err.message);
    }
}
// Function to send HTTP request to server 
async function forwardImage(socket, packet, clientIp) {
    const filename = `img_${Date.now()}.jpg`;

    fs.writeFile(filename, packet.payload, (err) => {
        if (err) {
            console.error('Image saving failed', err);
            socket.write('Image saving failed');
            return;
        } else {
            console.log('Image saved:', filename);
        }
    });

    try {
        await axios.post('http://localhost:3000/image', packet.payload, {
            headers: {
                'Content-Type': 'application/octet-stream',
                'X-Timestamp': packet.timestamp,
                //'X-Command-Arg': packet.command_arg,
                'X-Client': clientIp,
            }
        });
        console.log('Image forwarded to HTTP');
        socket.write('Image forwarded to HTTP');
    } catch (err) {
        console.error('Failed to forward image', err.message);
        socket.write('Failed to forward image');
    }
}

const server = net.createServer((socket) => {
    console.log('Connection established');

    socket.on('data', async (buffer) => {
        console.log('Received bytes:', buffer.length);

        const iv = buffer.subarray(0, 16);
        const encrypted = buffer.subarray(16);

        const decrypted = decryptPacket(iv, encrypted);
        const packet = new Packet(decrypted);
        
        const deviceIp = socket.remoteAddress();
    
        if (packet.command_id === 0x01) {
            sendChallengeResponse(socket, packet.payload);
            await deviceRegistration(socket, deviceIp);
        } else if (packet.command_id === 0x02) {
            await forwardImage(socket, packet, deviceIp);
        } else {
            socket.write('Invalid command');
        }
    });

    socket.on('end', () => {
        console.log('Disconnected');
    });
});

const PORT = 5001;
server.listen(PORT, () => {
    console.log(`TCP Server running on port ${PORT}`);
});
