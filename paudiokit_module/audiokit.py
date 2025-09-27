import _audiokit
from typing import Final

_ffi = _audiokit.ffi
_lib = _audiokit.lib

FILENAME : Final[str] = "./data/file_example_WAV_2MG.wav"
    
class Audiokit:
    def __init__(self):
        h = _ffi.new("struct HEADER *")

        _lib.retrieve_wav_data(FILENAME.encode("utf-8"), h)
        
        header = h[0]
        
        self.riff   = bytes(_ffi.buffer(header.riff, 4)).decode("ascii", errors="replace")
        self.wave   = bytes(_ffi.buffer(header.wave, 4)).decode("ascii", errors="replace")
        self.fmt    = bytes(_ffi.buffer(header.fmt_chunk_marker, 4)).decode("ascii", errors="replace")
        self.data   = bytes(_ffi.buffer(header.data_chunk_header, 4)).decode("ascii", errors="replace")
        self.overall_size     = int(header.overall_size)
        self.length_of_fmt    = int(header.length_of_fmt)
        self.format_type      = int(header.format_type)
        self.channels         = int(header.channels)
        self.sample_rate      = int(header.sample_rate)
        self.byterate         = int(header.byterate)
        self.block_align      = int(header.block_align)
        self.bits_per_sample  = int(header.bits_per_sample)
        self.data_size        = int(header.data_size)
        
if __name__ == "__main__":
    audiokit = Audiokit()
    print(f"audiokit channels : {audiokit.channels}")