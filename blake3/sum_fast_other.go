//go:build !amd64 || purego

package blake3

func sum256Fast(data []byte, keyWords [8]uint32, flags uint32) ([OutLen]byte, bool) {
	return [OutLen]byte{}, false
}
