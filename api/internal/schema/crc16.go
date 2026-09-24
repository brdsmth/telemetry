package schema

// CRC16CCITTFalse computes CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no
// reflection, no final xor) as used by every wire format in schema/PROTOCOL.md.
// The check value for "123456789" is 0x29B1.
func CRC16CCITTFalse(data []byte) uint16 {
	return crc16Continue(0xFFFF, data)
}

func crc16Continue(crc uint16, data []byte) uint16 {
	for _, b := range data {
		crc ^= uint16(b) << 8
		for i := 0; i < 8; i++ {
			if crc&0x8000 != 0 {
				crc = (crc << 1) ^ 0x1021
			} else {
				crc <<= 1
			}
		}
	}
	return crc
}
