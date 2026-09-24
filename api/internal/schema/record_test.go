package schema

import (
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

type vectorFile struct {
	Format string `json:"format"`
	Version int   `json:"version"`
	Size    int   `json:"size"`
	CRC     struct {
		CheckInput string `json:"check_input"`
		Check      string `json:"check"`
	} `json:"crc"`
	Valid []struct {
		Name   string `json:"name"`
		Hex    string `json:"hex"`
		Record struct {
			Version uint8   `json:"version"`
			Type    uint8   `json:"type"`
			Quality uint8   `json:"quality"`
			Flags   uint8   `json:"flags"`
			Seq     uint32  `json:"seq"`
			Time    uint32  `json:"time"`
			Value   float32 `json:"value"`
			BootID  uint16  `json:"boot_id"`
		} `json:"record"`
	} `json:"valid"`
	Invalid []struct {
		Name  string `json:"name"`
		Hex   string `json:"hex"`
		Error string `json:"error"`
	} `json:"invalid"`
}

func loadVectors(t *testing.T) vectorFile {
	t.Helper()
	path := filepath.Join("..", "..", "..", "schema", "vectors", "records.json")
	raw, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read vectors: %v", err)
	}
	var v vectorFile
	if err := json.Unmarshal(raw, &v); err != nil {
		t.Fatalf("parse vectors: %v", err)
	}
	if v.Format != "record" {
		t.Fatalf("unexpected vector format %q", v.Format)
	}
	return v
}

func mustHex(t *testing.T, s string) []byte {
	t.Helper()
	b, err := hex.DecodeString(s)
	if err != nil {
		t.Fatalf("bad hex %q: %v", s, err)
	}
	return b
}

func TestCRCCheckValue(t *testing.T) {
	v := loadVectors(t)
	got := CRC16CCITTFalse([]byte(v.CRC.CheckInput))
	want := mustHex(t, v.CRC.Check)
	if got != uint16(want[0])<<8|uint16(want[1]) {
		t.Fatalf("crc(%q) = %04x, want %s", v.CRC.CheckInput, got, v.CRC.Check)
	}
}

func TestConstantsMatchVectors(t *testing.T) {
	v := loadVectors(t)
	if v.Version != RecordVersion || v.Size != RecordSize {
		t.Fatalf("vectors are version %d size %d, code is version %d size %d",
			v.Version, v.Size, RecordVersion, RecordSize)
	}
}

func TestDecodeValidVectors(t *testing.T) {
	for _, vec := range loadVectors(t).Valid {
		t.Run(vec.Name, func(t *testing.T) {
			got, err := Decode(mustHex(t, vec.Hex))
			if err != nil {
				t.Fatalf("decode: %v", err)
			}
			want := Record{
				Version: vec.Record.Version,
				Type:    vec.Record.Type,
				Quality: vec.Record.Quality,
				Flags:   vec.Record.Flags,
				Seq:     vec.Record.Seq,
				Time:    vec.Record.Time,
				Value:   vec.Record.Value,
				BootID:  vec.Record.BootID,
			}
			if got != want {
				t.Fatalf("decode = %+v, want %+v", got, want)
			}
		})
	}
}

func TestEncodeValidVectorsByteForByte(t *testing.T) {
	for _, vec := range loadVectors(t).Valid {
		t.Run(vec.Name, func(t *testing.T) {
			r := Record{
				Version: vec.Record.Version,
				Type:    vec.Record.Type,
				Quality: vec.Record.Quality,
				Flags:   vec.Record.Flags,
				Seq:     vec.Record.Seq,
				Time:    vec.Record.Time,
				Value:   vec.Record.Value,
				BootID:  vec.Record.BootID,
			}
			got := Encode(r)
			if hex.EncodeToString(got[:]) != vec.Hex {
				t.Fatalf("encode = %x, want %s", got, vec.Hex)
			}
		})
	}
}

func TestInvalidVectorsReportNamedError(t *testing.T) {
	for _, vec := range loadVectors(t).Invalid {
		t.Run(vec.Name, func(t *testing.T) {
			_, err := Decode(mustHex(t, vec.Hex))
			if err == nil {
				t.Fatal("decode succeeded, want error")
			}
			if err.Error() != vec.Error {
				t.Fatalf("error = %q, want %q", err.Error(), vec.Error)
			}
		})
	}
}

func TestNames(t *testing.T) {
	if TypeName(1) != "soil_resistance_ohms" || TypeName(250) != "unknown" {
		t.Fatal("TypeName mismatch")
	}
	if QualityName(3) != "open" || QualityName(9) != "unknown" {
		t.Fatal("QualityName mismatch")
	}
}
