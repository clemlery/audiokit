import _audiokit
from typing import Final

_ffi = _audiokit.ffi
_lib = _audiokit.lib

FILENAME : Final[str] = "./data/file_example_WAV_2MG.wav"

def retrieve_wav_data() -> None:
	_lib.retrieve_wav_data(FILENAME.encode("utf-8"))

if __name__ == "__main__":
	retrieve_wav_data()
