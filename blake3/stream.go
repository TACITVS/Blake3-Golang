package blake3

import (
	"io"
	"os"
	"time"
)

const DefaultBufferSize = 256 * 1024
const maxEmptyReads = 8

type Progress struct {
	Processed uint64
	Total     uint64
	Elapsed   time.Duration
}

type ProgressFunc func(Progress)

// WriteReader streams data from r into the hasher using buf and reports progress.
// If total is unknown, pass 0. The callback can call h.Sum256() to snapshot the
// current digest when needed.
func (h *Hasher) WriteReader(r io.Reader, buf []byte, total uint64, onProgress ProgressFunc) (int64, error) {
	if len(buf) == 0 {
		buf = make([]byte, DefaultBufferSize)
	}

	start := time.Now()
	var processed uint64
	emptyReads := 0

	for {
		n, err := r.Read(buf)
		if n > 0 {
			emptyReads = 0
			_, _ = h.Write(buf[:n])
			processed += uint64(n)
			if onProgress != nil {
				onProgress(Progress{
					Processed: processed,
					Total:     total,
					Elapsed:   time.Since(start),
				})
			}
		}

		if err == io.EOF {
			if n == 0 && onProgress != nil {
				onProgress(Progress{
					Processed: processed,
					Total:     total,
					Elapsed:   time.Since(start),
				})
			}
			return int64(processed), nil
		}
		if err != nil {
			return int64(processed), err
		}
		if n == 0 {
			emptyReads++
			if emptyReads >= maxEmptyReads {
				return int64(processed), io.ErrNoProgress
			}
		}
	}
}

// HashReader streams a reader into a new hasher and returns the 32-byte digest.
func HashReader(r io.Reader, bufSize int, onProgress ProgressFunc) ([OutLen]byte, error) {
	h := New()
	buf := make([]byte, bufferSizeOrDefault(bufSize))
	_, err := h.WriteReader(r, buf, 0, onProgress)
	if err != nil {
		return [OutLen]byte{}, err
	}
	return h.Sum256(), nil
}

// HashFile streams a file into a new hasher and reports progress with total size.
func HashFile(path string, bufSize int, onProgress ProgressFunc) ([OutLen]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		return [OutLen]byte{}, err
	}
	defer f.Close()

	info, err := f.Stat()
	if err != nil {
		return [OutLen]byte{}, err
	}
	total := uint64(info.Size())

	h := New()
	buf := make([]byte, bufferSizeOrDefault(bufSize))
	_, err = h.WriteReader(f, buf, total, onProgress)
	if err != nil {
		return [OutLen]byte{}, err
	}
	return h.Sum256(), nil
}

func bufferSizeOrDefault(n int) int {
	if n > 0 {
		return n
	}
	return DefaultBufferSize
}
