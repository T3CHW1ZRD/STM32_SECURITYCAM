class Packet {
  constructor(buffer) {
    this.command_id = buffer.readUInt8(0);
    this.command_arg = buffer.readUInt16LE(1);
    this.data_len = buffer.readUInt32LE(3);
    this.padding_len = buffer.readUInt8(7);
    this.timestamp = buffer.readUInt32LE(8);
    this.payload = buffer.subarray(12, 12 + this.data_len);
  }
}

module.exports = Packet;
