package session

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"runtime"
	"strings"
	"sync"
	"testing"
)

type consoleRecorder struct {
	lines []string
}

func (recorder *consoleRecorder) Write(data []byte) (int, error) {
	runtime.Gosched()
	recorder.lines = append(recorder.lines, string(data))
	return len(data), nil
}

func TestPassThroughWriterCompleteLines(t *testing.T) {
	recorder := &consoleRecorder{}
	writer := NewPassThroughWriter(recorder, []byte("[A] "))
	for _, part := range []string{"first", " line\nsecond\npartial", " line\n\n"} {
		written, err := writer.Write([]byte(part))
		if err != nil || written != len(part) {
			t.Fatalf("Write() = %d, %v", written, err)
		}
		if part == "first" && len(recorder.lines) != 0 {
			t.Fatal("incomplete line was emitted")
		}
	}
	want := []string{"[A] first line\n", "[A] second\n", "[A] partial line\n", "[A] \n"}
	if fmt.Sprint(recorder.lines) != fmt.Sprint(want) {
		t.Fatalf("writes = %q, want %q", recorder.lines, want)
	}
}

type consoleDestination struct {
	output io.Writer
}

func (destination consoleDestination) Write(data []byte) (int, error) {
	return destination.output.Write(data)
}

func TestPassThroughWriterConcurrentStreams(t *testing.T) {
	recorder := &consoleRecorder{}
	var workers sync.WaitGroup
	for stream := 0; stream < 8; stream++ {
		writer := NewPassThroughWriter(consoleDestination{recorder}, fmt.Appendf(nil, "[%d] ", stream))
		workers.Add(1)
		go func() {
			defer workers.Done()
			for line := 0; line < 100; line++ {
				for _, part := range []string{"message", " body\n"} {
					if _, err := writer.Write([]byte(part)); err != nil {
						t.Error(err)
						return
					}
				}
			}
		}()
	}
	workers.Wait()
	if len(recorder.lines) != 800 {
		t.Fatalf("got %d writes, want 800", len(recorder.lines))
	}
	for _, line := range recorder.lines {
		if len(line) != len("[0] message body\n") || !strings.HasPrefix(line, "[") ||
			line[1] < '0' || line[1] > '7' || line[2:] != "] message body\n" {
			t.Fatalf("interleaved or incomplete line: %q", line)
		}
	}
}

type failingConsoleWriter struct {
	err error
}

func (writer failingConsoleWriter) Write(data []byte) (int, error) {
	return 0, writer.err
}

func TestPassThroughWriterOutputErrors(t *testing.T) {
	outputErr := errors.New("output failed")
	for _, test := range []struct {
		name string
		err  error
		want error
	}{
		{name: "failure", err: outputErr, want: outputErr},
		{name: "short write", want: io.ErrShortWrite},
	} {
		t.Run(test.name, func(t *testing.T) {
			writer := NewPassThroughWriter(failingConsoleWriter{test.err}, []byte("[A] "))
			if _, err := writer.Write([]byte("line\n")); !errors.Is(err, test.want) {
				t.Fatalf("Write() error = %v, want %v", err, test.want)
			}
			var output bytes.Buffer
			next := NewPassThroughWriter(&output, nil)
			if _, err := next.Write([]byte("next\n")); err != nil {
				t.Fatal(err)
			}
		})
	}
}
