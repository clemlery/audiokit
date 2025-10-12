from dataclasses import dataclass
import _audiokit
from typing import Any, Final, Optional, Callable, Union
import numpy as np
from numpy.lib.stride_tricks import as_strided


_ffi = _audiokit.ffi
_lib = _audiokit.lib

FILENAME: Final[str] = "./data/file_example_WAV_2MG.wav"


# ################################ HELPERS ################################


# Dataclass used to store all useful data about the wav file loaded.
@dataclass
class WaveData:
    riff: str
    wave: str
    fmt: str
    data_chunk_header: str
    overall_size: int
    length_of_fmt: int
    format_type: int
    channels: int
    sample_rate: int
    byterate: int
    block_align: int
    bits_per_sample: int
    data_size: int
    data: np.ndarray
    frame_number: int
    sample_number: int
    audio_length_s: float


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
        return bytes(_ffi.buffer(c_error_message, message_bytes_size)).decode(
            "ascii", errors="replace"
        )

    # We convert the error code outputed into a comprehensible python Exception
    def handle_output(output: int) -> None:
        """
        PEP - 257 format
        TODO
        """
        if output == 0:
            return

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
    def retrieve_wav_data(filename: str) -> WaveData:
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

        frame_number: int = int(c_frame)
        channels: int = int(c_header.num_channels)
        sample_number: int = frame_number * channels
        data_size: int = int(c_header.subchunk2_size)
        byterate: int = int(c_header.byte_rate)
        audio_length_s: float = data_size / byterate

        data = np.array(_ffi.unpack(c_data, sample_number))
        data_ch1 = data[::2]
        data_ch2 = data[1::2]

        # We create the dataclass to return
        wave_data = WaveData(
            riff=bytes(_ffi.buffer(c_header.chunk_id, 4)).decode(
                "ascii", errors="replace"
            ),
            wave=bytes(_ffi.buffer(c_header.format, 4)).decode(
                "ascii", errors="replace"
            ),
            fmt=bytes(_ffi.buffer(c_header.subchunk1_id, 4)).decode(
                "ascii", errors="replace"
            ),
            data_chunk_header=bytes(_ffi.buffer(c_header.subchunk2_id, 4)).decode(
                "ascii", errors="replace"
            ),
            overall_size=int(c_header.chunk_size),
            length_of_fmt=int(c_header.subchunk1_size),
            format_type=int(c_header.audio_format),
            channels=int(c_header.num_channels),
            sample_rate=int(c_header.sample_rate),
            byterate=byterate,
            block_align=int(c_header.block_align),
            bits_per_sample=int(c_header.bits_per_sample),
            data_size=data_size,
            data=np.array([data_ch1, data_ch2]),
            frame_number=int(frame_number),
            sample_number=sample_number,
            audio_length_s=audio_length_s,
        )

        return wave_data


class Audiokit:
    """
    Lightweight Python wrapper around the C-based audio loader and feature
    extractors used in this project.

    This class exposes parsed WAV metadata (header fields) and the decoded audio
    samples as NumPy arrays, and provides common analysis utilities such as
    Zero-Crossings and Zero-Crossing Rate (ZCR). All heavy I/O is delegated to
    the C backend via ``_audiokit`` for speed and low memory overhead.
    """

    def __init__(self, filename: str = ""):
        """
        Initialize an ``Audiokit`` instance by loading a WAV file and
        populating metadata and audio buffers.

        Parameters
        ----------
        filename : str, optional
            Path to a WAV file to load. If provided, the file is decoded via
            the C backend and all public attributes of the instance (header
            fields, sampling rate, and ``self.data``) are populated.

        Raises
        ------
        ValueError, IOError, MemoryError, RuntimeError
            Propagated from the C backend through ``ErrorHandler.handle_output``
            if loading fails (e.g., invalid format, I/O error, allocation
            failure, or other runtime issue).
        """
        wave_data: WaveData = AudiokitInterface.retrieve_wav_data(filename=filename)

        self.riff = wave_data.riff
        self.wave = wave_data.wave
        self.fmt = wave_data.fmt
        self.overall_size = wave_data.overall_size
        self.length_of_fmt = wave_data.length_of_fmt
        self.format_type = wave_data.format_type
        self.channels = wave_data.channels
        self.sample_rate = wave_data.sample_rate
        self.byterate = wave_data.byterate
        self.block_align = wave_data.block_align
        self.bits_per_sample = wave_data.bits_per_sample
        self.data_chunk_header = wave_data.data_chunk_header
        self.data_size = wave_data.data_size
        self.data = wave_data.data
        self.frame_number = wave_data.frame_number
        self.sample_number = wave_data.sample_number
        self.audio_length_s = wave_data.audio_length_s

    @staticmethod    
    def zero_crossing_rate(
        y: np.ndarray,
        *,
        frame_length: int = 2048,
        hop_length: int = 512,
        threshold: float = 1e-10,
        pad: bool = True,
        zero_pos: bool = True,
        axis: int = -1,
    ) -> np.ndarray:
        """
        Compute the zero-crossing rate (ZCR) over short-time frames.

        The input signal is first converted into a boolean zero-crossing mask via
        :meth:`Audiokit.zero_crossings`, then framed without copying using
        strided views, and finally averaged per frame to obtain the ZCR.

        Parameters
        ----------
        y : np.ndarray
            Input audio array. Can be 1D (``(T,)``) or multi-dimensional with
            the temporal axis specified by ``axis`` (e.g., ``(C, T)`` for
            multi-channel audio). ``float32`` is preferred for performance.
        frame_length : int, optional
            Number of samples per analysis frame. Defaults to ``2048``.
        hop_length : int, optional
            Number of samples to advance between successive frames. Defaults to
            ``512``.
        threshold : float, optional
            Absolute threshold below which samples are clamped to ``0.0`` prior
            to sign detection. Set to ``0`` to disable. Defaults to ``1e-10``.
        pad : bool, optional
            Initial value for the first sample of the zero-crossing mask. This
            mirrors ``librosa``'s behavior for alignment. Defaults to ``True``.
        zero_pos : bool, optional
            If ``True``, treat zero as non-negative (i.e., zero belongs to the
            positive side). If ``False``, use a tri-valued sign (``-1, 0, +1``).
            Defaults to ``True``.
        axis : int, optional
            Axis index of the temporal dimension. The computation is performed
            along this axis. Defaults to ``-1``.

        Returns
        -------
        np.ndarray
            Array of zero-crossing rates with the same leading dimensions as
            ``y`` but with the temporal axis replaced by the number of frames.
            For example, if ``y`` is ``(C, T)`` and ``axis=-1``, the output has
            shape ``(C, n_frames)`` where ``n_frames = 1 + (T-frame_length)//hop_length``.

        Notes
        -----
        This function uses stride-based framing (``as_strided``) to avoid copies;
        callers must ensure that ``frame_length`` and ``hop_length`` are chosen
        so that frames stay within bounds, i.e., ``frame_length <= T`` and the
        final frame end does not exceed the input length.
        """
        zc = Audiokit.zero_crossings(y, threshold=threshold, pad=pad, zero_pos=zero_pos, axis=axis)

        if axis != -1:
            zc = np.moveaxis(zc, axis, -1)

        n_frames = 1 + (zc.shape[-1] - frame_length) // hop_length
        shape = zc.shape[:-1] + (n_frames, frame_length)
        strides = zc.strides[:-1] + (hop_length * zc.strides[-1], zc.strides[-1])

        frames = np.lib.stride_tricks.as_strided(zc, shape=shape, strides=strides)

        zcr = frames.mean(axis=-1)

        if axis != -1:
            zcr = np.moveaxis(zcr, -1, axis)

        return zcr

    @staticmethod
    def zero_crossings(
        y: np.ndarray,
        *,
        threshold: float = 1e-10,
        pad: bool = True,
        zero_pos: bool = True,
        axis: int = -1,
    ) -> np.ndarray:
        """
        Return a boolean mask indicating zero-crossings at the sample level.

        A zero-crossing occurs when the sign of consecutive samples differs.
        This function produces a boolean array with the same shape as ``y``,
        where ``True`` marks a crossing at the corresponding sample index.

        Parameters
        ----------
        y : np.ndarray
            Input audio array. Can be 1D (``(T,)``) or multi-dimensional with
            the temporal axis specified by ``axis``. Values are treated as
            ``float32`` for performance; other dtypes are cast without copying
            when possible.
        threshold : float, optional
            Absolute threshold below which samples are clamped to ``0.0`` before
            sign detection. Set to ``0`` to disable thresholding. Defaults to
            ``1e-10``.
        pad : bool, optional
            Value assigned to the first sample along the temporal axis in the
            output mask. This emulates ``librosa``'s alignment convention.
            Defaults to ``True``.
        zero_pos : bool, optional
            If ``True``, consider zero as non-negative (i.e., zero belongs to
            the positive side) and detect crossings via the sign bit. If
            ``False``, compute a tri-valued sign (``-1, 0, +1``) so that
            transitions through exact zeros are accounted for explicitly.
            Defaults to ``True``.
        axis : int, optional
            Axis index of the temporal dimension. The computation is performed
            along this axis. Defaults to ``-1``.

        Returns
        -------
        np.ndarray
            Boolean array of the same shape as ``y`` where ``True`` indicates a
            zero-crossing at that position along the temporal axis.
        """
        y = np.asarray(y)
        if y.dtype != np.float32:
            y = y.astype(np.float32, copy=False)

        if axis != -1:
            y = np.moveaxis(y, axis, -1)

        if threshold > 0:
            y = y.copy()  
            np.putmask(y, np.abs(y) <= threshold, 0.0)

        out = np.empty_like(y, dtype=bool)  
        out[..., 0] = pad

        if zero_pos:
            neg = np.signbit(y) 
            np.logical_xor(neg[..., 1:], neg[..., :-1], out=out[..., 1:])
        else:
            s = np.empty_like(y, dtype=np.int8)
            s.fill(1)
            s[y < 0] = -1
            s[y == 0] = 0
            np.not_equal(s[..., 1:], s[..., :-1], out=out[..., 1:])

        if axis != -1:
            out = np.moveaxis(out, -1, axis)

        return out