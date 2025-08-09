const net = require('net');
const crypto = require('crypto');
const fs = require('fs');
const axios = require('axios');
const Packet = require('./Packet');
require('dotenv').config();

const key = Buffer.from(process.env.KEY, 'hex');
const algorithm = 'aes-192-cbc';

//photo flags from test server
const P_FLAG_START = 1 << 0;
const P_FLAG_LAST  = 1 << 1;
const P_SEQ_MASK   = 0xFFF0;
const P_SEQ_SHIFT  = 4;

const P_DEADLINE_MS   = 5000;               
const MAX_IMAGE_BYTES = 8 * 1024 * 1024;    // safety cap

function decryptPacket(iv, encrypted) {
  const decipher = crypto.createDecipheriv(algorithm, key, iv);
  return Buffer.concat([decipher.update(encrypted), decipher.final()]);
}

function encryptPacket(iv, decrypted){
    const cipher = crypto.createCipheriv(algorithm, key, iv);
    return Buffer.concat([cipher.update(encrypted), decipher.final()]);
}
function sendChallengeResponse(socket, payload) {
  socket.write(payload);
}

async function deviceRegistration(clientIp){
  try{
    await axios.post('http://localhost:3000/device', null, {
      headers:{ 'X-Client': `${clientIp}` }
    });
    console.log('Device registered with Backend');
  } catch(err){
    console.log('Failed to Register Device', err.message);
  }
}

async function forwardImage(imgBuffer, timestamp, clientIp) {
  // also save for testing
  const filename = `img_${Date.now()}.jpg`;
  try {
    fs.writeFileSync(filename, imgBuffer);
    console.log('Image saved:', filename);
  } catch (err) {
    console.error('Image saving failed', err);
  }

  // forward to HTTP
  try {
    await axios.post('http://localhost:3000/device/image', imgBuffer, {
      headers: {
        'Content-Type': 'application/octet-stream',
        'X-Timestamp': timestamp,
        'X-Client': clientIp,
      }
    });
    console.log('Image forwarded to HTTP');
  } catch (err) {
    console.error('Failed to forward image', err.message);
  }
}

function abortPhoto(photo, cause){
  if (!photo) return null;
  console.warn(`[photo] Aborting: ${cause}`);
  try { photo.fh?.destroy(); } catch {}
  if (photo.timer) clearTimeout(photo.timer);
  return null;
}

async function completePhoto(photo, deviceIp){
  if (!photo) return null;
  if (photo.timer) clearTimeout(photo.timer);
  try { photo.fh.end(); } catch {}
  console.log(`Completed ${photo.bytes} bytes -> ${photo.filename}`);

  await forwardImage(Buffer.concat(photo.imgBuffer), photo.timestamp, deviceIp);
  return null;
}

function refreshDeadline(photo, onTimeout){
  if (photo.timer) clearTimeout(photo.timer);
  photo.timer = setTimeout(onTimeout, P_DEADLINE_MS);
}

const server = net.createServer((socket) => {
  console.log('Connection established from', socket.remoteAddress);
  const deviceIp = socket.remoteAddress;
  let verified = false;
  let photo = null;
  let rx = Buffer.alloc(0);

  socket.on('data', async (buffer) => {
    rx = Buffer.concat([rx, buffer]);
    if (rx.length < 32) {
      console.warn('Buffer size short');
      return;
    }
    while(buffer.length >= 32){

        if(!verified){
            const iv = buffer.subarray(0, 16);
            const val = buffer.subarray(16, 32);
            const payload = encryptPacket(iv, val);
            sendChallengeResponse(socket, payload);
            verified = true;
            rx = rx.subarray(32);
        }

        const iv   = buffer.subarray(0, 16);
        const data = buffer.subarray(16);

        let decrypted;
        try {
        decrypted = decryptPacket(iv, data);
        } catch (e) {
        console.error('Decrypt failed:', e.message);
        return;
        }
        const packet = new Packet(decrypted);
        const size = 16 + packet.data_len + data.padding_len;
        const cipher_size = Math.ceil(size / 16) * 16;   // bytes after IV
        const total_need  = 16  + cipher_size ;
        if(rx.length < total_need) break;

        if (packet.command_id === 0x00) {
        sendChallengeResponse(socket, packet.payload);
        await deviceRegistration(deviceIp);
        return;
        }

        if (packet.command_id === 0x02) {
            await deviceRegistration(deviceIp);
        const flags = packet.command_arg & 0x00FF;
        const seq   = (packet.command_arg & P_SEQ_MASK) >> P_SEQ_SHIFT;
        const sid   = packet.timestamp;

        const onTimeout = () => {
            photo = abortPhoto(photo, 'chunk timeout');
        };

        if (flags & P_FLAG_START) {
            if (packet.data_len < 4) {
            console.warn('[photo] START with too-small payload');
            return;
            }

            const totalLen = packet.payload.readUInt32LE(0);
            if (totalLen > MAX_IMAGE_BYTES) {
            console.warn('[photo] totalLen exceeds cap, aborting');
            return;
            }

            const jpegPart = packet.payload.subarray(4);
            console.log(`[photo] START sid=${sid} total=${totalLen} seq=${seq} len=${jpegPart.length}`);

            // abort any previous session
            if (photo) photo = abortPhoto(photo, 'new START while busy');

            const filename = `photo_${sid}.jpg`;
            const fh = fs.createWriteStream(filename, { flags: 'w' });

            photo = {
            sid,
            total: totalLen,
            bytes: 0,
            exSeq: 1,                 
            fh,
            filename,
            timestamp: packet.timestamp,
            timer: null,
            imgBuffer: [],             
            };

            // write first part
            fh.write(jpegPart);
            photo.imgBuffer.push(Buffer.from(jpegPart));
            photo.bytes += jpegPart.length;

            refreshDeadline(photo, onTimeout);

            if ((flags & P_FLAG_LAST) || photo.bytes >= photo.total) {
            photo = await completePhoto(photo, deviceIp);
            }
            return;
        }

        
        if (!photo || sid !== photo.sid) {
            console.warn('[photo] CHUNK for unknown/expired session — ignoring');
            return;
        }

        // sequence check
        if (seq !== photo.exSeq) {
            console.error(`[photo] Sequence mismatch (got ${seq}, want ${photo.exSeq})`);
            photo = abortPhoto(photo, 'sequence mismatch');
            return;
        }

        // append chunk
        photo.fh.write(packet.payload);
        photo.imgBuffer.push(Buffer.from(packet.payload));
        photo.bytes += packet.payload.length;
        photo.exSeq += 1;

        refreshDeadline(photo, onTimeout);

        if ((flags & P_FLAG_LAST) || photo.bytes >= photo.total) {
            photo = await completePhoto(photo, deviceIp);
        }
        return;
        }
    console.log('Invalid command received from device:', packet.command_id);
    }
    rx = rx.subarray(32);
  });

  socket.on('end', () => {
    console.log('Disconnected');
    if (photo) photo = abortPhoto(photo, 'disconnect');
  });

  socket.on('error', (err) => {
    console.error('Socket error:', err.message);
    if (photo) photo = abortPhoto(photo, 'socket error');
  });
});


const PORT = 5001;
server.listen(PORT, () => {
  console.log(`TCP Server running on port ${PORT}`);
});
