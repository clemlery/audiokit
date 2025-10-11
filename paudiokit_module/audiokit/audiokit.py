from dataclasses import dataclass
import _audiokit
from typing import Any, Final, Optional, Callable, Union
import numpy as np
import librosa
from numpy.lib.stride_tricks import as_strided


_ffi = _audiokit.ffi
_lib = _audiokit.lib

FILENAME : Final[str] = "./data/file_example_WAV_2MG.wav"


# ################################ HELPERS ################################


# Librosa zero_crossings function implementation 
def zero_crossings(
    y : np.ndarray,
    *,
    threshold: float = 1e-10,
    pad : bool = True,
    zero_pos : bool = True,
    axis : int = -1) -> np.ndarray:
    y = np.array(y, copy=True)

    if threshold > 0:
        y[np.abs(y) <= threshold] = 0.0

    if zero_pos:
        s = np.where(y < 0, -1, 1)      
    else:
        s = np.zeros_like(y, dtype=int) 
        s[y > 0] = 1
        s[y < 0] = -1

    s = np.swapaxes(s, axis, -1)
    jumps = s[..., 1:] != s[..., :-1] 

    first = np.full(s.shape[:-1] + (1,), pad, dtype=bool)
    z = np.concatenate([first, jumps], axis=-1)
    z = np.swapaxes(z, axis, -1)
    return z

# Librosa frame function implementation:
def frame(
    x: np.ndarray,
    *,
    frame_length: int,
    hop_length: int,
    axis: int = -1,
    writeable: bool = False,
    subok: bool = False,
) -> np.ndarray:
    x = np.array(x, copy=False, subok=subok)

    if x.shape[axis] < frame_length:
        raise ValueError(
            f"Input is too short (n={x.shape[axis]:d}) for frame_length={frame_length:d}"
        )

    if hop_length < 1:
        raise ValueError(f"Invalid hop_length: {hop_length:d}")

    # put our new within-frame axis at the end for now
    out_strides = x.strides + tuple([x.strides[axis]])

    # Reduce the shape on the framing axis
    x_shape_trimmed = list(x.shape)
    x_shape_trimmed[axis] -= frame_length - 1

    out_shape = tuple(x_shape_trimmed) + tuple([frame_length])
    xw = as_strided(
        x, strides=out_strides, shape=out_shape, subok=subok, writeable=writeable
    )

    if axis < 0:
        target_axis = axis - 1
    else:
        target_axis = axis + 1

    xw = np.moveaxis(xw, -1, target_axis)

    # Downsample along the target axis
    slices = [slice(None)] * xw.ndim
    slices[axis] = slice(0, None, hop_length)
    return xw[tuple(slices)]

    

def zero_crossing_rate(
    y: np.ndarray,
    *,
    frame_length: int = 2048,
    hop_length: int = 512,
    center: bool = True,
    **kwargs: Any,
) -> np.ndarray:
    if center:
        padding = [(0, 0) for _ in range(y.ndim)]
        padding[-1] = (int(frame_length // 2), int(frame_length // 2))
        y = np.pad(y, padding, mode="edge")

    y_framed = frame(y, frame_length=frame_length, hop_length=hop_length)

    kwargs["axis"] = -2
    kwargs.setdefault("pad", False)

    crossings = zero_crossings(y_framed, **kwargs)

    zcrate: np.ndarray = np.mean(crossings, axis=-2, keepdims=True)
    return zcrate

# Dataclass used to store all useful data about the wav file loaded.
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
    data : np.ndarray
    frame_number : int
    sample_number : int
    audio_length_s : float


# Class used to handling errors code outputed by the function C side.
class ErrorHandler:
    def __init__(self):
        pass
    
    # We retrieve the last error message define C side 
    def get_last_error_message() -> str:
        """
        PEP - 257 format
        TODO 
        """
        c_error_message = _lib.last_error_message()
        message_bytes_size = _ffi.sizeof(c_error_message)
        return bytes(_ffi.buffer(c_error_message, message_bytes_size)).decode("ascii", errors="replace")
    
    # We convert the error code outputed into a comprehensible python Exception
    def handle_output(output : int) -> None:
        """
        PEP - 257 format
        TODO 
        """
        if output == 0: return
        
        last_error_message = ErrorHandler.get_last_error_message()

        """ TODO commentary for each exception """

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
        """
        PEP - 257 format
        TODO 
        """
        
        # We initialize the pointer of type struct HEADER that 
        h = _ffi.new("struct wav_header *")
        s = _ffi.new("float **")
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
        data_size : int = int(c_header.subchunk2_size)
        byterate : int = int(c_header.byte_rate)
        audio_length_s : float = data_size/byterate
        
        data = np.array(_ffi.unpack(c_data, sample_number))
        data_ch1 = data[::2]
        data_ch2 = data[1::2]
        
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
            byterate=byterate,
            block_align=int(c_header.block_align),
            bits_per_sample=int(c_header.bits_per_sample),
            data_size=data_size,
            data= np.array([data_ch1, data_ch2]),
            frame_number= int(frame_number),
            sample_number=sample_number,
            audio_length_s=audio_length_s
        )
        
        return wave_data
    
    @staticmethod
    def zero_crossing_rate(data : np.ndarray, frame_number : int, frame_length : int, hop_length : int, center : int) -> np.ndarray:
        """
        PEP - 257 format
        TODO 
        """
        
        z = _ffi.new("float **")
        f = _ffi.new("size_t *")
        
        c_data = _ffi.cast('int16_t*', data.ctypes.data)
        
        output = _lib.zero_crossing_rate(c_data, frame_number, frame_length, hop_length, center, z, f)
        
        ErrorHandler.handle_output(output)
        
        c_zcr = z[0]
        n_frame = int(f[0])
                
        return np.array(_ffi.unpack(c_zcr, n_frame))
            
class Audiokit:
    def __init__(self, filename : str = ""):
        """
        PEP - 257 format
        TODO 
        """
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
        self.audio_length_s = wave_data.audio_length_s
        
    def zero_crossing_rate(self, frame_length : int, hop_length : int, center : int) -> np.ndarray:
        """
        PEP - 257 format
        TODO 
        """
        return np.array(
            [AudiokitInterface.zero_crossing_rate(self.data[0], self.frame_number, frame_length, hop_length, center),
            AudiokitInterface.zero_crossing_rate(self.data[1], self.frame_number, frame_length, hop_length, center)]
        )
                
if __name__ == "__main__":
    
    audiokit = Audiokit(FILENAME)
    y, sr = librosa.load(FILENAME, mono=False, sr=None)

    print(f'data librosa shape : {y.shape}')
    print(f'data audiokit shape : {audiokit.data.shape}')
    
    audiokit_channel1_values = audiokit.data[0][:5000]
    
    librosa_channel1_values = y[0][:5000]
    
    librosa_chn_size = len(librosa_channel1_values)
    audiokit_chn_size = len(audiokit_channel1_values)
    
    if len(librosa_channel1_values) != len(audiokit_channel1_values):
        raise ValueError(f"Array of channels 1 don't have the same size : librosa's array size : {librosa_chn_size}, audiokit's array size : {audiokit_chn_size}")
        
    for i in range(librosa_chn_size):
        if librosa_channel1_values[i] != audiokit_channel1_values[i]:
            raise ValueError(f"Values are not equal at index : {i}")
        
    zcr_librosa = librosa.feature.zero_crossing_rate(
        y=librosa_channel1_values,
        frame_length=500,
        hop_length=100,
        center=False
    )
    
    zcr_implentation = zero_crossing_rate(
        y=audiokit_channel1_values,
        frame_length=500,
        hop_length=100,
        center=False
    )
    
    print(f"lirosa zcr shape : {zcr_librosa.shape}")
    print(f"python implementation zcr shape : {zcr_librosa.shape}")
    
    
    zcr_nb = len(zcr_librosa[0])
    
    for i in range(zcr_nb):
        print(f"{i}. librosa zcr value : {zcr_librosa[0][i]} ; python zcr value : {zcr_implentation[0][i]}")

    # audiokit = Audiokit(FILENAME)
        
    # y, sr = librosa.load(FILENAME, sr=None, mono=False)
    
    # print(f'librosa array shape : {np.shape(y)}')
    # print(f'audiokit array shape : {np.shape(audiokit.data)}')
    
    # zcr_audiokit = audiokit.zero_crossing_rate(
    #     frame_length=2048,
    #     hop_length=512,
    #     center=1
    # )
    
    # zcr_librosa = librosa.feature.zero_crossing_rate(
    #     y=y,
    #     frame_length=2048,
    #     hop_length=512,
    #     center=False
    # )
    
    # print(f'librosa zcr shape : {np.shape(zcr_librosa)}')
    # print(f'audiokit zcr shape : {np.shape(zcr_audiokit)}')
    
    
    # for i in range(10):
    #     print(f'{i}. librosa ch1 : {zcr_librosa[0][0][i]} ch2 : {zcr_librosa[1][0][i]}')
    #     print(f'{i}. audiokit ch1 : {zcr_audiokit[0][i]} ch2 : {zcr_audiokit[1][i]}')
    
    # for i in range(10):
    #     print(f'{i}. librosa data : {y[0][i]}')
    #     print(f'{i}. audiokit data : {audiokit.data[0][i]}')
        