Tiny Lib
========

A collection of various header-only libraries for C++.

The common spirit of the libraries is to keep them friendly to be used on an
embeddable 32bit systems which might not have memory management unit (MMU).

Some of the libraries implement new algorithms, others solve the problem of
platform-dependent behavior of low-level C/C++ functions.

Libraries
---------

Here is an overview of the available libraries. Every library contains detailed
information at the top of their header files.

Library                                                   | Description
--------------------------------------------------------- | -----------
[tl_audio_wav_reader](tl_audio_wav/tl_audio_wav_reader.h) | Reader of WAVE files
[tl_audio_wav_writer](tl_audio_wav/tl_audio_wav_writer.h) | Writer of WAVE files
[tl_build_config](tl_build_config/tl_build_config.h)      | Compile-time detection of compiler and hardware platform configuration
[tl_static_vector](tl_container/tl_static_vector.h)       | A fixed capacity dynamically sized vector
[tl_convert](tl_convert/tl_convert.h)                     | Various string to arithmetic conversion utilities
[tl_callback](tl_functional/tl_callback.h)                | Simple implementation of a callback with an attachable listeners
[tl_image_bmp_reader](tl_image_bmp/tl_image_bmp_reader.h) | Simple implementation of BMP reader
[tl_image_bmp_writer](tl_image_bmp/tl_image_bmp_writer.h) | Simple implementation of BMP writer
[tl_io_file](tl_io/tl_io_file.h)                          | File read and write implementation
[tl_log](tl_log/tl_log.h)                                 | Building blocks for logging which happens to a application-dependent output
[tl_result](tl_result/tl_result.h)                        | An optional contained value with an error information associated with it
[tl_cstring_view](tl_string/tl_cstring_view.h)            | A C compatible string_view adapter
[tl_static_string](tl_string/tl_static_string.h)          | A fixed capacity dynamically sized string
[tl_string_portable](tl_string/tl_string_portable.h)      | Versions of POSIX string functions which ensures predictable behavior
[tl_temp_dir](tl_temp/tl_temp_dir.h)                      | Cross-platform RAII helper for managing temp directory
[tl_temp_file](tl_temp/tl_temp_file.h)                    | Cross-platform RAII helper for managing temp file

License
-------

The Tiny Lib sources are licensed under the terms of the MIT-0 license.
See [LICENSE](LICENSE) for more information.
