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
          LIBRARIES tl_audio_wav)
endfunction()

tl_audio_wav_test(reader)
tl_audio_wav_test(writer)
