package blake3

import (
	"encoding/hex"
	"encoding/json"
	"os"
	"testing"
)

type testVectors struct {
	Key           string `json:"key"`
	ContextString string `json:"context_string"`
	Cases         []struct {
		InputLen  int    `json:"input_len"`
		Hash      string `json:"hash"`
		KeyedHash string `json:"keyed_hash"`
		DeriveKey string `json:"derive_key"`
	} `json:"cases"`
}

func patternBytes(n int) []byte {
	out := make([]byte, n)
	for i := 0; i < n; i++ {
		out[i] = byte(i % 251)
	}
	return out
}

func TestVectors(t *testing.T) {
	raw, err := os.ReadFile("testdata/test_vectors.json")
	if err != nil {
		t.Fatalf("read test vectors: %v", err)
	}
	var vectors testVectors
	if err := json.Unmarshal(raw, &vectors); err != nil {
		t.Fatalf("parse test vectors: %v", err)
	}

	if len(vectors.Key) != KeyLen {
		t.Fatalf("unexpected key length: %d", len(vectors.Key))
	}
	var key [KeyLen]byte
	copy(key[:], vectors.Key)

	for _, tc := range vectors.Cases {
		input := patternBytes(tc.InputLen)

		hashOut := make([]byte, len(tc.Hash)/2)
		hasher := New()
		_, _ = hasher.Write(input)
		hasher.Finalize(hashOut)
		if got := hex.EncodeToString(hashOut); got != tc.Hash {
			t.Fatalf("hash mismatch len=%d\nwant=%s\ngot =%s", tc.InputLen, tc.Hash, got)
		}
		sum := hasher.Sum256()
		if got := hex.EncodeToString(sum[:]); got != tc.Hash[:OutLen*2] {
			t.Fatalf("sum mismatch len=%d\nwant=%s\ngot =%s", tc.InputLen, tc.Hash[:OutLen*2], got)
		}

		keyedOut := make([]byte, len(tc.KeyedHash)/2)
		keyed := NewKeyed(key)
		_, _ = keyed.Write(input)
		keyed.Finalize(keyedOut)
		if got := hex.EncodeToString(keyedOut); got != tc.KeyedHash {
			t.Fatalf("keyed hash mismatch len=%d\nwant=%s\ngot =%s", tc.InputLen, tc.KeyedHash, got)
		}

		derivedOut := make([]byte, len(tc.DeriveKey)/2)
		derived := NewDeriveKey(vectors.ContextString)
		_, _ = derived.Write(input)
		derived.Finalize(derivedOut)
		if got := hex.EncodeToString(derivedOut); got != tc.DeriveKey {
			t.Fatalf("derive key mismatch len=%d\nwant=%s\ngot =%s", tc.InputLen, tc.DeriveKey, got)
		}
	}
}

func TestChunkedWrites(t *testing.T) {
	input := patternBytes(4096)
	full := Sum256(input)

	hasher := New()
	for offset := 0; offset < len(input); {
		chunk := 1
		if remain := len(input) - offset; remain > 7 {
			chunk = (offset % 7) + 1
		}
		end := offset + chunk
		if end > len(input) {
			end = len(input)
		}
		_, _ = hasher.Write(input[offset:end])
		offset = end
	}
	got := hasher.Sum256()
	if got != full {
		t.Fatalf("chunked mismatch\nwant=%x\ngot =%x", full, got)
	}
}
