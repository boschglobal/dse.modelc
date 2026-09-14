package convert

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/boschglobal/dse.schemas/code/go/ab/stream/pdu"
	flatbuffers "github.com/google/flatbuffers/go"
)

func TestNewNetworkMeasurementAddsSuffix(t *testing.T) {
	tests := []struct {
		name     string
		input    string
		suffix   string
		wantFile string
	}{
		{
			name:     "adds suffix before extension",
			input:    "capture.asc",
			suffix:   "network",
			wantFile: "capture.network.asc",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			dir := t.TempDir()
			traceFile := filepath.Join(dir, tc.input)

			got := NewNetworkMeasurement(tc.suffix, traceFile, 42)

			want := filepath.Join(dir, tc.wantFile)
			if got.filename != want {
				t.Fatalf("expected filename %q, got %q", want, got.filename)
			}
		})
	}
}

func TestWriteCanEventFormatsExtendedFrameAsExpected(t *testing.T) {
	tests := []struct {
		name         string
		format       pdu.CanMessageFormat
		payload      []byte
		msgEcuID     uint32
		txEcuID      uint32
		wantContains []string
	}{
		{
			name:         "fd extended frame tx",
			format:       pdu.CanMessageFormatFdExtendedFrameFormat,
			payload:      []byte{0x01, 0x02, 0x03},
			msgEcuID:     42,
			txEcuID:      42,
			wantContains: []string{"CANFD", " 8 ", "Tx", "1234x", "01 02 03"},
		},
		{
			name:         "standard base frame rx",
			format:       pdu.CanMessageFormatBaseFrameFormat,
			payload:      []byte{0xAA, 0xBB},
			msgEcuID:     10,
			txEcuID:      42, // Mismatch produces Rx
			wantContains: []string{" 8 ", "Rx", "1234", "aa bb"},
		},
		{
			name:         "standard extended frame tx",
			format:       pdu.CanMessageFormatExtendedFrameFormat,
			payload:      []byte{0x10, 0x20},
			msgEcuID:     42,
			txEcuID:      42,
			wantContains: []string{" 8 ", "Tx", "1234x", "10 20"},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			dir := t.TempDir()
			outPath := filepath.Join(dir, "event.asc")
			file, err := os.Create(outPath)
			if err != nil {
				t.Fatalf("create output file: %v", err)
			}

			nm := &NetworkMeasurement{file: file, txEcuId: tc.txEcuID}
			msg := buildTestPdu(t, tc.format, tc.payload, tc.msgEcuID)

			if err := nm.writeCanEvent(1.234567, msg); err != nil {
				_ = file.Close()
				t.Fatalf("writeCanEvent returned error: %v", err)
			}
			if err := file.Close(); err != nil {
				t.Fatalf("close output file: %v", err)
			}

			data, err := os.ReadFile(outPath)
			if err != nil {
				t.Fatalf("read output file: %v", err)
			}

			line := strings.TrimSpace(string(data))
			if strings.HasPrefix(line, " ") {
				t.Fatalf("expected timestamp to be left-aligned without leading padding, got %q", line)
			}
			if tc.format == pdu.CanMessageFormatFdExtendedFrameFormat && !strings.Contains(line, "CANFD") {
				t.Fatalf("expected CANFD field in output, got %q", line)
			}
			for _, want := range tc.wantContains {
				if !strings.Contains(line, want) {
					t.Fatalf("expected output to contain %q, got %q", want, line)
				}
			}
		})
	}
}

func buildTestPdu(t *testing.T, format pdu.CanMessageFormat, payload []byte, ecuID uint32) *pdu.Pdu {
	t.Helper()

	builder := flatbuffers.NewBuilder(0)

	payVec := builder.CreateByteVector(payload)

	pdu.CanMessageMetadataStart(builder)
	pdu.CanMessageMetadataAddMessageFormat(builder, format)
	pdu.CanMessageMetadataAddNetworkId(builder, 7)
	canMeta := pdu.CanMessageMetadataEnd(builder)

	pdu.PduStart(builder)
	pdu.PduAddId(builder, 0x1234)
	pdu.PduAddPayload(builder, payVec)
	pdu.PduAddTransportType(builder, pdu.TransportMetadataCan)
	pdu.PduAddTransport(builder, canMeta)
	pdu.PduAddSwcId(builder, 1)
	pdu.PduAddEcuId(builder, ecuID)
	pduOffset := pdu.PduEnd(builder)

	builder.Finish(pduOffset)

	buf := builder.FinishedBytes()
	return pdu.GetRootAsPdu(buf, 0)
}
