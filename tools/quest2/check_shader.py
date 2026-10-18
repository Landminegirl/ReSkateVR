"""Compile the embedded VR shaders using Windows D3DCompile, without a headset."""
import ctypes as c
from pathlib import Path


def release(blob):
    table = c.cast(blob, c.POINTER(c.POINTER(c.c_void_p))).contents
    c.WINFUNCTYPE(c.c_ulong, c.c_void_p)(table[2])(blob)


def blob_text(blob):
    table = c.cast(blob, c.POINTER(c.POINTER(c.c_void_p))).contents
    pointer = c.WINFUNCTYPE(c.c_void_p, c.c_void_p)(table[3])(blob)
    size = c.WINFUNCTYPE(c.c_size_t, c.c_void_p)(table[4])(blob)
    return c.string_at(pointer, size).decode('utf-8', errors='replace')


def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / 'Extension/VR/vr_stereo.cpp').read_text(encoding='utf-8')
    shader = source.split('R"(', 1)[1].split(')";', 1)[0].encode('utf-8')
    compile_shader = c.WinDLL('d3dcompiler_47.dll').D3DCompile
    compile_shader.restype = c.c_int32
    compile_shader.argtypes = [c.c_void_p, c.c_size_t, c.c_char_p, c.c_void_p,
                              c.c_void_p, c.c_char_p, c.c_char_p, c.c_uint,
                              c.c_uint, c.POINTER(c.c_void_p), c.POINTER(c.c_void_p)]
    for entry, target in ((b'vs_main', b'vs_5_0'), (b'ps_main', b'ps_5_0'),
                          (b'ps_dilate', b'ps_5_0')):
        code, errors = c.c_void_p(), c.c_void_p()
        result = compile_shader(shader, len(shader), b'reskate_vr_stereo', None, None,
                                entry, target, 1 << 15, 0, c.byref(code), c.byref(errors))
        message = blob_text(errors) if errors else ''
        if errors:
            release(errors)
        if code:
            release(code)
        if result < 0:
            raise RuntimeError(f'{entry.decode()}: {message}')
        print(f'{entry.decode()}: compiled successfully')


if __name__ == '__main__':
    main()
