// Package schema implements the wire formats defined in schema/PROTOCOL.md.
// It is tested against the golden vectors in schema/vectors and must stay in
// lock step with the firmware and app codecs.
package schema

import (
	"encoding/binary"
	"errors"
	"math"
)

const (
	ProtocolVersion = 1
	RecordVersion   = 1
	RecordSize      = 20
)

// Reading types, schema/PROTOCOL.md §1.1. Decoders accept ids not listed.
const (
	TypeSoilResistanceOhms uint8 = 1
	TypeBatteryMillivolts  uint8 = 2
	TypeBoardTemperatureC  uint8 = 3
)

// Quality, schema/PROTOCOL.md §1.2.
const (
	QualityOk   uint8 = 0
	QualityLow  uint8 = 1
	QualityHigh uint8 = 2
	QualityOpen uint8 = 3
)

// Flags, schema/PROTOCOL.md §1.3.
const FlagEpochValid uint8 = 0x01

// Record is one reading. Time is unix seconds when EpochValid is set,
// otherwise seconds since the boot identified by BootID.
type Record struct {
	Version uint8
	Type    uint8
	Quality uint8
	Flags   uint8
	Seq     uint32
	Time    uint32
	Value   float32
	BootID  uint16
}

func (r Record) EpochValid() bool { return r.Flags&FlagEpochValid != 0 }

// Decode errors. Their Error() strings match the `error` field of the
// invalid vectors in schema/vectors/records.json.
var (
	ErrShortInput         = errors.New("short_input")
	ErrUnsupportedVersion = errors.New("unsupported_version")
	ErrBadCRC             = errors.New("bad_crc")
)

const crcOffset = RecordSize - 2

// Encode produces the 20-byte wire form, computing the CRC. The Version field
// is written as given so tests can produce invalid records.
func Encode(r Record) [RecordSize]byte {
	var out [RecordSize]byte
	out[0] = r.Version
	out[1] = r.Type
	out[2] = r.Quality
	out[3] = r.Flags
	binary.LittleEndian.PutUint32(out[4:], r.Seq)
	binary.LittleEndian.PutUint32(out[8:], r.Time)
	binary.LittleEndian.PutUint32(out[12:], math.Float32bits(r.Value))
	binary.LittleEndian.PutUint16(out[16:], r.BootID)
	binary.LittleEndian.PutUint16(out[crcOffset:], CRC16CCITTFalse(out[:crcOffset]))
	return out
}

// Decode checks length, then version, then CRC, in that order.
func Decode(in []byte) (Record, error) {
	if len(in) < RecordSize {
		return Record{}, ErrShortInput
	}
	if in[0] != RecordVersion {
		return Record{}, ErrUnsupportedVersion
	}
	if CRC16CCITTFalse(in[:crcOffset]) != binary.LittleEndian.Uint16(in[crcOffset:]) {
		return Record{}, ErrBadCRC
	}
	return Record{
		Version: in[0],
		Type:    in[1],
		Quality: in[2],
		Flags:   in[3],
		Seq:     binary.LittleEndian.Uint32(in[4:]),
		Time:    binary.LittleEndian.Uint32(in[8:]),
		Value:   math.Float32frombits(binary.LittleEndian.Uint32(in[12:])),
		BootID:  binary.LittleEndian.Uint16(in[16:]),
	}, nil
}

// TypeName returns the schema name for a reading type, or "unknown".
func TypeName(t uint8) string {
	switch t {
	case TypeSoilResistanceOhms:
		return "soil_resistance_ohms"
	case TypeBatteryMillivolts:
		return "battery_millivolts"
	case TypeBoardTemperatureC:
		return "board_temperature_c"
	}
	return "unknown"
}

// QualityName returns the schema name for a quality byte, or "unknown".
func QualityName(q uint8) string {
	switch q {
	case QualityOk:
		return "ok"
	case QualityLow:
		return "low"
	case QualityHigh:
		return "high"
	case QualityOpen:
		return "open"
	}
	return "unknown"
}
