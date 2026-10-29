// GPL-2.0-or-later. Synthetic inputs only; no commercial audio data.
#include <bit_reader.h>
#include <libatrac9.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

static void test_bits(void) {
    // An exactly one-byte allocation exposes speculative multi-byte overreads.
    for (unsigned value = 0; value < 256; ++value) {
        unsigned char byte = (unsigned char)value;
        BitReaderCxt br;
        jmp_buf overflow;
        InitBitReaderCxt(&br, &byte);
        br.BitLimit = 8;
        br.Overflow = &overflow;
        assert(setjmp(overflow) == 0);
        assert(PeekInt(&br, 25) == (int)(value << 17));
        for (int bit = 7; bit >= 0; --bit)
            assert(ReadInt(&br, 1) == (int)((value >> bit) & 1));
    }
    unsigned char byte = 0xff;
    BitReaderCxt br;
    jmp_buf overflow;
    InitBitReaderCxt(&br, &byte);
    br.BitLimit = 8;
    br.Overflow = &overflow;
    if (setjmp(overflow) == 0) {
        (void)ReadInt(&br, 9);
        assert(!"truncated read must abort");
    }
}

int main(void) {
    test_bits();
    int used = 123;
    short pcm[2048] = {0};
    unsigned char data = 0;
    assert(Atrac9DecodeBounded(NULL, &data, 1, pcm, &used) != 0 && used == 0);
    // Valid synthetic mono configuration with a deliberately truncated frame.
    for (unsigned value = 0; value < 256; ++value) {
        unsigned char config[4] = {0xfe, 0x70, 0, 0};
        void *handle = Atrac9GetHandle();
        assert(handle && Atrac9InitDecoder(handle, config) == 0);
        data = (unsigned char)value;
        used = 123;
        assert(Atrac9DecodeBounded(handle, &data, 1, pcm, &used) != 0);
        assert(used == 0);
        Atrac9ReleaseHandle(handle);
    }
    puts("ATRAC9 bounded reader and 256 truncated frames passed");
    return 0;
}
