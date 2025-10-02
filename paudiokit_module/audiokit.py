from dataclasses import dataclass
import _audiokit
from typing import Final
import numpy as np
import matplotlib.pyplot as plt

_ffi = _audiokit.ffi
_lib = _audiokit.lib

FILENAME : Final[str] = "./data/file_example_WAV_2MG.wav"

# ################################ HELPERS ################################

@dataclass
class WaveData:
    riff : str
    wave : str
    fmt : str
    data_chunk_header : str
    overall_size : int
    length_of_fmt : int
    format_type : int
    channels : int
    sample_rate : int
    byterate : int
    block_align : int
    bits_per_sample : int
    data_size : int
    data : list[int]
    frame_number : int
    sample_number : int

class ErrorHandler:
    def __init__(self):
        pass
    
    def get_last_error_message() -> str:
        c_error_message = _lib.last_error_message()
        message_bytes_size = _ffi.sizeof(c_error_message)
        return bytes(_ffi.buffer(c_error_message, message_bytes_size)).decode("ascii", errors="replace")
    
    def handle_output(output : int) -> None:
        if output == 0: return
        
        last_error_message = ErrorHandler.get_last_error_message()        

        match output:
            case 1:
                raise ValueError(last_error_message)
            case 2:
                raise IOError(last_error_message)
            case 3:
                raise ValueError(last_error_message)
            case 4:
                raise MemoryError(last_error_message)
            case 5:
                raise RuntimeError(last_error_message)
            
# Interface contributing to link C functions with Python Call  
class AudiokitInterface:
    def __init__(self) -> None:
        pass
    
    @staticmethod
    def retrieve_wav_data(filename : str) -> WaveData:
        # We initialize the pointer of type struct HEADER that 
        h = _ffi.new("struct wav_header *")
        s = _ffi.new("int16_t **")
        f = _ffi.new("uint32_t *")
        
        # We read the wav file and retrieve all of his data (header and content)
        # The output is used as an error indicator (0 : OK, > 0 : Error)
        output = int(_lib.retrieve_wav_data(filename.encode("utf-8"), h, s, f))
                
        # Handling errors
        ErrorHandler.handle_output(output)
        
        # We retrieve the header struct from the output pointer 
        c_header = h[0]
        c_data = s[0]
        c_frame = f[0]
        
        frame_number : int = int(c_frame)
        channels : int = int(c_header.num_channels)
        sample_number : int = frame_number*channels
        
        # We create the dataclass to return 
        wave_data = WaveData(
            riff=bytes(_ffi.buffer(c_header.chunk_id, 4)).decode("ascii", errors="replace"),
            wave=bytes(_ffi.buffer(c_header.format, 4)).decode("ascii", errors="replace"),
            fmt=bytes(_ffi.buffer(c_header.subchunk1_id, 4)).decode("ascii", errors="replace"),
            data_chunk_header=bytes(_ffi.buffer(c_header.subchunk2_id, 4)).decode("ascii", errors="replace"),
            overall_size=int(c_header.chunk_size),
            length_of_fmt=int(c_header.subchunk1_size),
            format_type=int(c_header.audio_format),
            channels=int(c_header.num_channels),
            sample_rate=int(c_header.sample_rate),
            byterate=int(c_header.byte_rate),
            block_align=int(c_header.block_align),
            bits_per_sample=int(c_header.bits_per_sample),
            data_size=int(c_header.subchunk2_size),
            data= np.array(_ffi.unpack(c_data, sample_number)),
            frame_number= int(frame_number),
            sample_number=sample_number
        )
        
        return wave_data
    
class Audiokit:
    def __init__(self, filename : str = ""):
        
        wave_data : WaveData = AudiokitInterface.retrieve_wav_data(filename=filename)
        
        self.riff   = wave_data.riff
        self.wave   = wave_data.wave
        self.fmt    = wave_data.fmt
        self.overall_size     = wave_data.overall_size
        self.length_of_fmt    = wave_data.length_of_fmt
        self.format_type      = wave_data.format_type
        self.channels         = wave_data.channels
        self.sample_rate      = wave_data.sample_rate
        self.byterate         = wave_data.byterate
        self.block_align      = wave_data.block_align
        self.bits_per_sample  = wave_data.bits_per_sample
        self.data_chunk_header = wave_data.data_chunk_header
        self.data_size        = wave_data.data_size
        self.data = wave_data.data
        self.frame_number = wave_data.frame_number
        self.sample_number = wave_data.sample_number
        
if __name__ == "__main__":
    audiokit = Audiokit(FILENAME)
    print(f"audiokit channels : {audiokit.channels}")

    
        