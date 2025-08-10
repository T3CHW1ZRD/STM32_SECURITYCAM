const net = require('net');
const crypto = require('crypto');
const fs = require('fs');
const axios = require('axios');
require('dotenv').config();

const keyHex = (process.env.KEY || '').trim();
if (!/^[0-9a-fA-F]{48}$/.test(keyHex)) {
  throw new Error('KEY must be 48 hex chars (24 bytes for AES-192)');
}
const key = Buffer.from(keyHex, 'hex');
const algorithm = 'aes-192-cbc';

// photo flags from MCU
const P_FLAG_START = 1 << 0;
const P_FLAG_LAST  = 1 << 1;
const P_SEQ_MASK   = 0xFFF0;
const P_SEQ_SHIFT  = 4;

const P_DEADLINE_MS   = 5000;
const MAX_IMAGE_BYTES = 8 * 1024 * 1024;

function decryptPacket(iv, encrypted) {
  const d = crypto.createDecipheriv(algorithm, key, iv);
  d.setAutoPadding(false); // manual padding in protocol
  return Buffer.concat([d.update(encrypted), d.final()]);
}

function encryptPacket(iv, plain) {
  const c = crypto.createCipheriv(algorithm, key, iv);
  c.setAutoPadding(false); // manual padding in protocol
  return Buffer.concat([c.update(plain), c.final()]);
}

// Build exact GET_PHOTO packet like Python (pad_len = 4, no payload)
function buildGetPhotoPacket(timestamp = 0, fixedIv = null) {
  const cmdId   = 0x01;   // GET_PHOTO
  const cmdArg  = 0;
  const pad_len = 4;      // header(12) + 0 + 4 = 16

  const header = Buffer.alloc(12);
  header.writeUInt8(cmdId, 0);
  header.writeUInt16LE(cmdArg, 1);
  header.writeUInt32LE(0, 3);            // data_len = 0
  header.writeUInt8(pad_len, 7);
  header.writeUInt32LE(timestamp >>> 0, 8);

  const plain = Buffer.concat([header, Buffer.alloc(pad_len)]); // 16 bytes
  const iv = fixedIv ? Buffer.from(fixedIv) : crypto.randomBytes(16);
  const ct = encryptPacket(iv, plain);
  return Buffer.concat([iv, ct]); // 32 bytes
}

async function deviceRegistration(clientIp){
  try {
    await axios.post('http://localhost:3000/device', null, {
      headers: { 'X-Client': `${clientIp}` }
    });
    console.log('Device registered with Backend');
  } catch(err){
    console.log('Failed to Register Device', err.message);
  }
}

async function forwardImage(imgBuffer, timestamp, clientIp) {
  const filename = `img_${Date.now()}.jpg`;
  try {
    fs.writeFileSync(filename, imgBuffer);
    console.log('Image saved:', filename);
  } catch (err) {
    console.error('Image saving failed', err);
  }

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
  socket.setNoDelay(true); // reduce latency for tight MCU timeouts
  const deviceIp = socket.remoteAddress;

  let verified = false;
  let photo = null;
  let rx = Buffer.alloc(0);
  let pendingGetPhoto = false;

  socket.on('data', async (chunk) => {
    rx = Buffer.concat([rx, chunk]);

    // --- handshake ---
    if (!verified) {
      if (rx.length < 32) return;
      console.log('[HANDSHAKE] Got 32 bytes, performing handshake...');
      const challenge = rx.subarray(0, 16);
      const iv        = rx.subarray(16, 32);

      try {
        const resp = encryptPacket(iv, challenge); // encrypt challenge with given IV
        socket.write(resp);
        console.log(`[HANDSHAKE] Sent response (${resp.length} bytes)`);
      } catch (e) {
        console.error('Handshake encrypt failed:', e.message);
        socket.destroy(); return;
      }
      verified = true;
      rx = rx.subarray(32); // consume handshake bytes
      await deviceRegistration(deviceIp);
    }

    // --- packets ---
    while (verified) {
      if (rx.length < 32) break; // need IV + first block

      const iv = rx.subarray(0, 16);
      const c0 = rx.subarray(16, 32);

      let p0;
      try {
        const d0 = crypto.createDecipheriv(algorithm, key, iv);
        d0.setAutoPadding(false);
        p0 = Buffer.concat([d0.update(c0), d0.final()]);
      } catch (e) {
        console.error('[ERROR] Header decrypt failed:', e.message);
        rx = rx.subarray(1); // resync
        continue;
      }

      const cmd_id    = p0.readUInt8(0);
      const cmd_arg   = p0.readUInt16LE(1);
      const data_len  = p0.readUInt32LE(3);
      const pad_len   = p0.readUInt8(7);
      const timestamp = p0.readUInt32LE(8);

      console.log(`[PACKET] cmd_id=${cmd_id} arg=${cmd_arg} data_len=${data_len} pad_len=${pad_len} ts=${timestamp}`);

      const plain_size  = 12 + data_len + pad_len;
      if (plain_size < 12) {
        console.warn('[WARN] Bad plain_size:', plain_size);
        rx = rx.subarray(1); // resync
        continue;
      }

      const cipher_size = Math.ceil(plain_size / 16) * 16;
      const total_need  = 16 + cipher_size;

      if (rx.length < total_need) {
        console.log(`[WAIT] Have ${rx.length} bytes, need ${total_need} for full packet`);
        break;
      }

      const ciphertext = rx.subarray(16, 16 + cipher_size);

      let full_plain;
      try {
        const dec = crypto.createDecipheriv(algorithm, key, iv);
        dec.setAutoPadding(false);
        full_plain = Buffer.concat([dec.update(ciphertext), dec.final()]);
      } catch (e) {
        console.error('[ERROR] Packet decrypt failed:', e.message);
        rx = rx.subarray(1); // resync
        continue;
      }

      const payload = full_plain.subarray(12, 12 + data_len);
      
      // ---- dispatch ----
      let consumed = false;
      try {
        if (cmd_id === 0x00) {
          console.log('[CMD 0x00] Device registration');
          socket.write(payload);
          await deviceRegistration(deviceIp);
          consumed = true;
        }
        else if (cmd_id === 0x03) {
          console.log('[CMD 0x03] Alarm triggered → queue GET_PHOTO');
          pendingGetPhoto = true;  // send after we consume this frame
          consumed = true;
        }
        else if (cmd_id === 0x02) {
          console.log('[CMD 0x02] Photo chunk');
          const flags = cmd_arg & 0x00FF;
          const seq   = (cmd_arg & P_SEQ_MASK) >> P_SEQ_SHIFT;
          const sid   = timestamp;
          const onTimeout = () => { photo = abortPhoto(photo, 'chunk timeout'); };

          if (flags & P_FLAG_START) {
            if (payload.length < 4) {
              console.warn('[PHOTO] Too-small START payload');
              consumed = true; // drop this packet
              continue;
            }
            const totalLen = payload.readUInt32LE(0);
            const jpegPart = payload.subarray(4);
            
            if (totalLen <= 0 || totalLen > MAX_IMAGE_BYTES) {
              console.warn('[PHOTO] totalLen invalid:', totalLen);
              consumed = true;
              continue;
            }

            if (photo) photo = abortPhoto(photo, 'new START while busy');

            const filename = `photo_${sid}.jpg`;
            const fh = fs.createWriteStream(filename, { flags: 'w' });
            photo = { sid, total: totalLen, bytes: 0, exSeq: 1, fh, filename, timestamp, timer: null, imgBuffer: [] };
            if (jpegPart.length) {
              fh.write(jpegPart);
              photo.imgBuffer.push(Buffer.from(jpegPart));
              photo.bytes += jpegPart.length;
            }
            refreshDeadline(photo, onTimeout);

            if ((flags & P_FLAG_LAST) || photo.bytes >= photo.total) {
              photo = await completePhoto(photo, deviceIp);
            }
            consumed = true;
          } else {
            if (!photo || sid !== photo.sid) {
              console.warn('[PHOTO] CHUNK for unknown/expired session — ignoring (sid=%s have=%s)', sid, photo?.sid);
              consumed = true;
              continue;
            }
            if (seq !== photo.exSeq) {
              console.error(`[PHOTO] Sequence mismatch (got ${seq}, want ${photo.exSeq})`);
              photo = abortPhoto(photo, 'sequence mismatch');
              consumed = true;
              continue;
            }
            console.log(`[PHOTO] CHUNK sid=${sid} seq=${seq} len=${payload.length}`);
            if (payload.length) {
              photo.fh.write(payload);
              photo.imgBuffer.push(Buffer.from(payload));
              photo.bytes += payload.length;
            }
            photo.exSeq += 1;
            refreshDeadline(photo, onTimeout);

            if ((flags & P_FLAG_LAST) || photo.bytes >= photo.total) {
              photo = await completePhoto(photo, deviceIp);
            }
            consumed = true;
          }
        }
        else {
          console.warn('[WARN] Invalid command ID:', cmd_id);
          consumed = true;
        }
      } finally {
        rx = consumed ? rx.subarray(total_need) : rx.subarray(1);
      }
    }

    // After consuming frames from this TCP chunk, send queued GET_PHOTO
    if (pendingGetPhoto) {
      pendingGetPhoto = false;
      process.nextTick(() => {
        const pkt = buildGetPhotoPacket();
        console.log('[SEND] GET_PHOTO…');
        const ok = socket.write(pkt, () => console.log('[SEND] GET_PHOTO write callback'));
        if (!ok) socket.once('drain', () => console.log('[SEND] drained'));
      });
    }
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
