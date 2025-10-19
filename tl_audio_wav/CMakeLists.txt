# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_audio_wav_reader.h
  tl_audio_wav_writer.h
)

add_library(tl_audio_wav INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

function(tl_audio_wav_test PRIMITIVE_NAME)
  tl_test(audio_wav_${PRIMITIVE_NAME}
          test/tl_audio_wav_${PRIMITIVE_NAME}_test.cc
          test/tl_audio_wav_test_data.h
          LIBRARIES tl_audio_wav)
endfunction()

tl_test(audio_wav_reader_buffered test/tl_audio_wav_reader_test.cc
        LIBRARIES tl_audio_wav)
tl_test(audio_wav_reader_unbuffered test/tl_audio_wav_reader_test.cc
  LIBRARIES tl_audio_wav
  DEFINITIONS TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE=0
              TL_AUDIO_WAV_READER_BUFFER_SIZE=0
)

tl_test(audio_wav_writer_buffered test/tl_audio_wav_writer_test.cc
        LIBRARIES tl_audio_wav)
tl_test(audio_wav_writer_unbuffered test/tl_audio_wav_writer_test.cc
  LIBRARIES tl_audio_wav
  DEFINITIONS TL_AUDIO_WAV_WRITER_SAMPLE_FRAME_BUFFER_SIZE=0
              TL_AUDIO_WAV_WRITER_BUFFER_SIZE=0
)
