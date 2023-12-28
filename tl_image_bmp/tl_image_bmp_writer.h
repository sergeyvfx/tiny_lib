// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Simple implementation of BMP writer which does not use memory allocation and
// uses a custom IO provider to perform disk IO.
//
//
// Example
// =======
//
//   const FormatSpec format_spec = {
//       .width = 320,
//       .height = 240,
//       .num_bits_per_pixel = 32,
//   };
//
//   const PixelsSpec pixels_spec = {
//       .num_channels = 3,
//   };
//
//   File file;
//   Writer<File> writer;
//
//   writer.Open(file, format_spec);
//   writer.Write(pixels_spec, pixels);
//   writer.Close();
//
//
// File Writer
// ===========
//
// The file writer is to implement a Write() method which can be invoked with
// the following signature:
//
//   auto Write(const void* ptr, SizeType num_bytes_to_write) -> IntType;
//
// It is possible to have trailing arguments with default values in the
// method if they do not change the expected invoke arguments.
//
// The num_bytes_to_write denotes the number of bytes which the BMP writer
// expects to be written to the file.
//
// Upon success the Write() is to return the number of bytes actually written.
//
// The BMP writer expects the number of actually written bytes to match the
// number of requested bytes to be written. This gives flexibility to the error
// handling in the file writer implementation: both write() and fwrite() style
// of the return value in the case of error or reading past the EOF will work
// for the BMP writer.
//
//
// Limitations
// ===========
//
// - Only 24 bits per channel files are supported.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <span>

// Semantic version of the tl_image_bmp_writer library.
#define TL_IMAGE_BMP_WRITER_VERSION_MAJOR 0
#define TL_IMAGE_BMP_WRITER_VERSION_MINOR 0
#define TL_IMAGE_BMP_WRITER_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_IMAGE_BMP_WRITER_NAMESPACE
#  define TL_IMAGE_BMP_WRITER_NAMESPACE tiny_lib::image_bmp_writer
#endif

// Helpers for TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)     \
  v_##id1##_##id2##_##id3
#define TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT(id1, id2, id3)            \
  TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE -> v_0_1_9
#define TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE                                  \
  TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT(                                \
      TL_IMAGE_BMP_WRITER_VERSION_MAJOR,                                       \
      TL_IMAGE_BMP_WRITER_VERSION_MINOR,                                       \
      TL_IMAGE_BMP_WRITER_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_IMAGE_BMP_WRITER_NAMESPACE {
inline namespace TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Public declarations.

// Specification of how pixels are stored in the BMP file.
class FormatSpec {
 public:
  // Width and height of the image, in pixels.
  int width{-1};
  int height{-1};

  // The number of bits per pixel, which is the color depth of the image.
  // Typical values are 1, 4, 8, 16, 24 and 32.
  int num_bits_per_pixel{-1};

  // True when the scanlines are stored top-to-bottom in the file.
  bool is_stored_top_to_bottom{false};

  // Get the number of channels in the file.
  inline auto GetNumChannels() const -> int {
    switch (num_bits_per_pixel) {
      case 24: return 3;
      case 32: return 4;
    }
    return 0;
  }
};

// Specification of how pixels are stored in memory.
//
// Defines distance between adjacent elements of a picture measured in multiples
// of a single channel type.
class PixelsSpec {
 public:
  // The number of channel.
  // Expected to be 1, 2, 3, or 4.
  //
  // The order of channels is always defined as RGBA.
  //
  // When 0 the number of channels from the file specification will be used.
  int num_channels{0};

  // Number of single channel elements between first elements of two adjacent
  // pixels.
  //
  // When is set to 0 matches the resolved number of channels.
  int pixel_stride{0};

  // Number of single channel elements between first pixels of two adjacent
  // rows.
  //
  // When 0 the value is calculated from the image width in the file
  // specification and the resolved pixel stride.
  int row_stride{0};
};

template <class FileWriter>
class Writer {
 public:
  // Constructs a new BMP file writer.
  // The writer is not ready for writing and is to be opened first.
  Writer() = default;

  Writer(Writer&& other) noexcept = default;
  auto operator=(Writer&& other) -> Writer& = default;

  // Delete copy constructor and assignment operator.
  //
  // Simplifies the FileWriter API by avoiding requirement to implement the
  // copy file descriptor API call.
  Writer(const Writer& other) noexcept = delete;
  auto operator=(const Writer& other) -> Writer& = delete;

  ~Writer() = default;

  // Open file for writing pixels with the given specification into the file
  // writer.
  //
  // The file writer is referenced by the BMP writer and caller is to guarantee
  // it is valid throughout the BMP file writing.
  //
  // Multiple calls of Open() on the same file is undefined.
  //
  // Returns true upon success.
  auto Open(FileWriter& file_writer, const FormatSpec& format_spec) -> bool;

  // Write pixels from the given memory of the given layout specification to
  // file.
  //
  // There is no internal check for the number of pixels written to the file,
  // so it is up to the caller to ensure proper number of pixels are written.
  //
  // It is possible to write the file incrementally as more and more pixels are
  // known by calling Write() multiple times. The requirements for this are:
  //
  //   - The number of the provided pixels is a multiple of row stride.
  //
  //   - The Write() call happens in the same order as the order of the storage
  //     (top-to-bottom or bottom-to-top).
  //
  // NOTE: Pixels are always expected to be provided in the top-to-bottom order.
  //
  // Returns true on success.
  auto Write(const PixelsSpec& pixels_spec, std::span<const uint8_t> pixels)
      -> bool;

  // Close the file writer.
  //
  // Calling on a writer which is not opened is undefined.
  auto Close() -> bool;

  // Simple pipeline of writing file in one go.
  //
  // Returns true on success.
  static auto Write(FileWriter& file_writer,
                    const FormatSpec& format_spec,
                    const PixelsSpec& pixels_spec,
                    std::span<const uint8_t> pixels) -> bool;

 private:
  // Write given object to file as-is without endian conversion.
  template <class T>
  inline auto WriteObjectToFile(const T& object) -> bool;

  // Convert byte order from native to the one stored in the file.
  template <class T>
  inline void NativeToFileEndian(T& object);

  // Write object from memory into file, converting its endian from native to
  // the file one.
  template <class T>
  inline auto WriteObjectToFileEndian(const T& object) -> bool;

  // Write header of the format specification to the file.
  // Returns true on success.
  inline auto WriteHeader() -> bool;

  // Resolve the storage specification to the actual numbers.
  inline auto ResolvePixelsSpec(const PixelsSpec& pixels_spec) -> PixelsSpec;

  // Write pixels from the given storage of given specification to RGB888 file.
  auto WriteToRGB888(const PixelsSpec& pixels_spec,
                     std::span<const uint8_t> pixels) -> bool;

  // An implementation of above with a given pixel accessor functor.
  // Allows to move channels count check to the outer loop.
  template <class AccessPixelFunctor>
  auto WriteToRGB888(const PixelsSpec& pixels_spec,
                     std::span<const uint8_t> pixels,
                     const AccessPixelFunctor& access_pixel) -> bool;

  // Write pixels from the given storage of given specification to ARGB888 file.
  auto WriteToARGB888(const PixelsSpec& pixels_spec,
                      std::span<const uint8_t> pixels) -> bool;

  // An implementation of above with a given pixel accessor functor.
  // Allows to move channels count check to the outer loop.
  template <class AccessPixelFunctor>
  auto WriteToARGB888(const PixelsSpec& pixels_spec,
                      std::span<const uint8_t> pixels,
                      const AccessPixelFunctor& access_pixel) -> bool;

  FileWriter* file_writer_{nullptr};

  // Endian in which multi-byte values are stored in the file.
  std::endian file_endian_{std::endian::little};

  bool is_open_attempted_{false};  // True if Open() has been called.
  bool is_open_{false};            // True if the Open() returned true.

  FormatSpec format_spec_;

  // Number of bytes used to store the row of pixels in the file.
  // The BMP format rounds up storage size of rows to 4 bytes.
  uint32_t row_size_in_bytes_{0};
};

////////////////////////////////////////////////////////////////////////////////
// Public API implementation.

template <class FileWriter>
auto Writer<FileWriter>::Open(FileWriter& file_writer,
                              const FormatSpec& format_spec) -> bool {
  assert(!is_open_attempted_);
  is_open_attempted_ = true;

  file_writer_ = &file_writer;
  format_spec_ = format_spec;

  // Calculate the row size with its padding.
  row_size_in_bytes_ =
      (format_spec.num_bits_per_pixel * format_spec.width + 31) / 32 * 4;

  if (!WriteHeader()) {
    return false;
  }

  is_open_ = true;

  return true;
}

template <class FileWriter>
auto Writer<FileWriter>::Write(const PixelsSpec& pixels_spec,
                               std::span<const uint8_t> pixels) -> bool {
  assert(is_open_);

  const PixelsSpec actual_pixels_spec = ResolvePixelsSpec(pixels_spec);

  switch (format_spec_.num_bits_per_pixel) {
    case 24: return WriteToRGB888(actual_pixels_spec, pixels);
    case 32: return WriteToARGB888(actual_pixels_spec, pixels);
  }

  // Unknown or unsupported bit depth.
  return false;
}

template <class FileWriter>
auto Writer<FileWriter>::Close() -> bool {
  assert(is_open_);

  is_open_ = false;
  file_writer_ = nullptr;

  // TODO(sergey): Verify all pixels has been provided.

  return true;
}

template <class FileWriter>
auto Writer<FileWriter>::Write(FileWriter& file_writer,
                               const FormatSpec& format_spec,
                               const PixelsSpec& pixels_spec,
                               std::span<const uint8_t> pixels) -> bool {
  Writer<FileWriter> writer;
  if (!writer.Open(file_writer, format_spec)) {
    return false;
  }

  if (!writer.Write(pixels_spec, pixels)) {
    return false;
  }

  return writer.Close();
}

////////////////////////////////////////////////////////////////////////////////
// Private implementation.

namespace internal {

// Endian conversion.

template <class T>
inline auto Byteswap(T value) -> T;

// Swap endianess of 8 bit unsigned int value.
template <>
inline auto Byteswap(const uint8_t value) -> uint8_t {
  return value;
}
template <>
inline auto Byteswap(const int8_t value) -> int8_t {
  return value;
}

// Swap endianess of 16 bit integer value.
template <>
inline auto Byteswap(const uint16_t value) -> uint16_t {
#if defined(__GNUC__)
  return __builtin_bswap16(value);
#elif defined(_MSC_VER)
  static_assert(sizeof(unsigned short) == 2, "Size of short should be 2");
  return _byteswap_ushort(value);
#else
  return (value >> 8) | (value << 8);
#endif
}
template <>
inline auto Byteswap(const int16_t value) -> int16_t {
  return static_cast<int16_t>(Byteswap(static_cast<uint16_t>(value)));
}

// Swap endianess of 32 bit integer value.
template <>
inline auto Byteswap(const uint32_t value) -> uint32_t {
#if defined(__GNUC__)
  return __builtin_bswap32(value);
#elif defined(_MSC_VER)
  static_assert(sizeof(unsigned long) == 4, "Size of long should be 4");
  return _byteswap_ulong(value);
#else
  return ((value >> 24)) | ((value << 8) & 0x00ff0000) |
         ((value >> 8) & 0x0000ff00) | ((value << 24));
#endif
}
template <>
inline auto Byteswap(const int32_t value) -> int32_t {
  return static_cast<int32_t>(Byteswap(static_cast<uint32_t>(value)));
}

// Utility macros to take burned of difference between GNU and MSVC style of
// packed structured definition.
//
// Allows to use simpler code like:
//
//  TL_IMAGE_BMP_PACK(struct MyStructName {
//    int32_t int32_field;
//    uint8_t uint8_t field;
//  });
//
// TODO(sergey): This is still not ideal since it will not work if there is a
// comma in the declaration.

#if defined(_MSC_VER)
#  define TL_IMAGE_BMP_PACK(__Declaration__)                                   \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#else
#  define TL_IMAGE_BMP_PACK(__Declaration__)                                   \
    __Declaration__ __attribute__((__packed__))
#endif

// BMP data structures, as per specification.
//
//   https://wikipedia.org/wiki/BMP_file_format
//
// The MSDN has explanation of the data structures as well.
//
// All of the integer values are stored in little-endian format (i.e.
// least-significant byte first).
//
// Higher level overview is available in MSDN:
//
//   https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-storage

// Suported values for the FileHeader::type.
// Example: `FileHeaderType::BM` for BMP images.
class FileHeaderType {
 public:
#define DEFINE_TYPE_CONSTANT(type)                                             \
  inline static constexpr uint16_t type = ((#type)[1] << 8) | (#type)[0]

  // Windows 3.1x, 95, NT, ... etc.
  DEFINE_TYPE_CONSTANT(BM);

  // OS/2 struct bitmap array.
  DEFINE_TYPE_CONSTANT(BA);

  // OS/2 struct color icon.
  DEFINE_TYPE_CONSTANT(CI);

  // OS/2 const color pointer.
  DEFINE_TYPE_CONSTANT(CP);

  // OS/2 struct icon.
  DEFINE_TYPE_CONSTANT(IC);

  // OS/2 pointer
  DEFINE_TYPE_CONSTANT(PT);

#undef DEFINE_TYPE_CONSTANT

  static auto IsKnown(const uint16_t type) -> bool {
    return type == BM || type == BA || type == CI || type == CP || type == IC ||
           type == PT;
  }
};

TL_IMAGE_BMP_PACK(struct FileHeader {
  // The header field used to identify the BMP and DIB file is 0x42 0x4D in
  // hexadecimal, same as BM in ASCII.
  // See constants in the `FileHeaderType` for possible values.
  uint16_t type;

  // The size of the BMP file in bytes.
  uint32_t size;

  // Reserved; actual value depends on the application that creates the image,
  // if created manually can be 0.
  int16_t reserved1;

  // Reserved; actual value depends on the application that creates the image,
  // if created manually can be 0.
  int16_t reserved2;

  // The offset, i.e. starting address, of the byte where the bitmap image data
  // (pixel array) can be found.
  // This is number of bytes from the beginning of the FileHeader structure.
  uint32_t offset_to_pixel_array;
});
static_assert(sizeof(FileHeader) == 14);

template <>
inline auto Byteswap(const FileHeader value) -> FileHeader {
  FileHeader result;
  result.type = Byteswap(value.type);
  result.size = Byteswap(value.size);
  result.reserved1 = Byteswap(value.reserved1);
  result.reserved2 = Byteswap(value.reserved2);
  result.offset_to_pixel_array = Byteswap(value.offset_to_pixel_array);
  return result;
}

class Compression {
 public:
  // NOLINTBEGIN(readability-identifier-naming)

  // Compression method: none.
  // Most common.
  inline static constexpr uint32_t BI_RGB = 0;

  // Compression method: RLE 8-bit/pixel.
  // Can be used only with 8-bit/pixel bitmaps.
  inline static constexpr uint32_t BI_RLE8 = 1;

  // Compression method: RLE 4-bit/pixel.
  // Can be used only with 4-bit/pixel bitmaps.
  inline static constexpr uint32_t BI_RLE4 = 2;

  // Compression method: OS22XBITMAPHEADER: Huffman 1D.
  // BITMAPV2INFOHEADER: RGB bit field masks,
  // BITMAPV3INFOHEADER+: RGBA
  inline static constexpr uint32_t BI_BITFIELDS = 3;

  // Compression method: OS22XBITMAPHEADER: RLE-24.
  // BITMAPV4INFOHEADER+: JPEG image for printing[
  inline static constexpr uint32_t BI_JPEG = 4;

  // BITMAPV4INFOHEADER+: PNG image for printing.
  inline static constexpr uint32_t BI_PNG = 5;

  // Compression method: RGBA bit field masks.
  // Only Windows CE 5.0 with .NET 4.0 or later.
  inline static constexpr uint32_t BI_ALPHABITFIELDS = 6;

  // Compression method: none.
  // Only Windows Metafile CMYK.
  inline static constexpr uint32_t BI_CMYK = 11;

  // Compression method: RLE-8.
  // Only Windows Metafile CMYK.
  inline static constexpr uint32_t BI_CMYKRLE8 = 12;

  // Compression method: RLE-4.
  // Only Windows Metafile CMYK.
  inline static constexpr uint32_t BI_CMYKRLE4 = 13;

  // NOLINTEND(readability-identifier-naming)
};

TL_IMAGE_BMP_PACK(struct InfoHeader {
  // The size of this header, in bytes (40).
  uint32_t size;

  // The bitmap width and height in pixels (signed integer).
  //
  // Negative height means bitmap is stored top-to-bottom, positive means it
  // is stored bottom-to-top.
  int32_t width;
  int32_t height;

  // the number of color planes (must be 1).
  uint16_t planes;

  // The number of bits per pixel, which is the color depth of the image.
  // Typical values are 1, 4, 8, 16, 24 and 32.
  uint16_t num_bits_per_pixel;

  // The compression method being used.
  // See `Compression` class for known compression type.
  uint32_t compression;

  // The image size. This is the size of the raw bitmap data; a dummy 0 can be
  // given for BI_RGB bitmaps.
  uint32_t image_size;

  // The horizontal and vertical resolution of the image. (pixel per metre,
  // signed integer)
  int32_t num_x_pixels_per_meter;
  int32_t num_y_pixels_per_meter;

  // The number of colors in the color palette, or 0 to default to 2n
  uint32_t num_colors_in_palette;

  // The number of important colors used, or 0 when every color is important;
  // generally ignored.
  uint32_t num_important_colors;
});
static_assert(sizeof(InfoHeader) == 40);

template <>
inline auto Byteswap(const InfoHeader value) -> InfoHeader {
  InfoHeader result;

  result.size = Byteswap(value.size);
  result.width = Byteswap(value.width);
  result.height = Byteswap(value.height);
  result.planes = Byteswap(value.planes);
  result.num_bits_per_pixel = Byteswap(value.num_bits_per_pixel);
  result.compression = Byteswap(value.compression);
  result.image_size = Byteswap(value.image_size);
  result.num_x_pixels_per_meter = Byteswap(value.num_x_pixels_per_meter);
  result.num_y_pixels_per_meter = Byteswap(value.num_y_pixels_per_meter);
  result.num_colors_in_palette = Byteswap(value.num_colors_in_palette);
  result.num_important_colors = Byteswap(value.num_important_colors);

  return result;
}

#undef TL_IMAGE_BMP_PACK

}  // namespace internal

template <class FileWriter>
template <class T>
inline auto Writer<FileWriter>::WriteObjectToFile(const T& object) -> bool {
  const void* object_ptr = static_cast<const void*>(&object);
  constexpr size_t kObjectSize = sizeof(T);

  return file_writer_->Write(object_ptr, kObjectSize) == kObjectSize;
}

template <class FileReader>
template <class T>
inline void Writer<FileReader>::NativeToFileEndian(T& object) {
  if (std::endian::native != file_endian_) {
    object = Byteswap(object);
  }
}

template <class FileReader>
template <class T>
inline auto Writer<FileReader>::WriteObjectToFileEndian(const T& object)
    -> bool {
  T file_endian = object;
  NativeToFileEndian(file_endian);

  return WriteObjectToFile(file_endian);
}

template <class FileReader>
inline auto Writer<FileReader>::WriteHeader() -> bool {
  const int width = format_spec_.width;
  const int height = format_spec_.height;
  const size_t pixels_size_in_bytes = size_t(height) * row_size_in_bytes_;

  // File header.
  internal::FileHeader file_header;
  file_header.type = internal::FileHeaderType::BM;
  file_header.size = sizeof(internal::FileHeader) +
                     sizeof(internal::InfoHeader) + pixels_size_in_bytes;
  file_header.reserved1 = 0;
  file_header.reserved2 = 0;
  file_header.offset_to_pixel_array =
      sizeof(internal::FileHeader) + sizeof(internal::InfoHeader);
  if (!WriteObjectToFileEndian(file_header)) {
    return false;
  }

  // Info header.
  internal::InfoHeader info_header;
  info_header.size = sizeof(internal::InfoHeader);
  info_header.width = width;
  info_header.height = format_spec_.is_stored_top_to_bottom ? -height : height;
  info_header.planes = 1;
  info_header.num_bits_per_pixel = format_spec_.num_bits_per_pixel;
  info_header.compression = internal::Compression::BI_RGB;
  info_header.image_size = pixels_size_in_bytes;
  info_header.num_x_pixels_per_meter = 0;
  info_header.num_y_pixels_per_meter = 0;
  info_header.num_colors_in_palette = 0;
  info_header.num_important_colors = 0;
  if (!WriteObjectToFileEndian(info_header)) {
    return false;  // NOLINT(readability-simplify-boolean-expr)
  }

  return true;
}

template <class FileReader>
inline auto Writer<FileReader>::ResolvePixelsSpec(const PixelsSpec& pixels_spec)
    -> PixelsSpec {
  PixelsSpec resolved = pixels_spec;

  if (resolved.num_channels == 0) {
    resolved.num_channels = format_spec_.GetNumChannels();
  }

  if (resolved.pixel_stride == 0) {
    resolved.pixel_stride = resolved.num_channels;
  }

  if (resolved.row_stride == 0) {
    resolved.row_stride = resolved.pixel_stride * format_spec_.width;
  }

  return resolved;
}

template <class FileReader>
auto Writer<FileReader>::WriteToRGB888(const PixelsSpec& pixels_spec,
                                       std::span<const uint8_t> pixels)
    -> bool {
  if (pixels_spec.num_channels == 1) {
    return WriteToRGB888(
        pixels_spec,
        pixels,
        [](const uint8_t* pixel, uint8_t& r, uint8_t& g, uint8_t& b) {
          r = pixel[0];
          g = pixel[0];
          b = pixel[0];
        });
  }

  if (pixels_spec.num_channels == 2) {
    return WriteToRGB888(
        pixels_spec,
        pixels,
        [](const uint8_t* pixel, uint8_t& r, uint8_t& g, uint8_t& b) {
          r = pixel[0];
          g = pixel[1];
          b = 0;
        });
  }

  return WriteToRGB888(
      pixels_spec,
      pixels,
      [](const uint8_t* pixel, uint8_t& r, uint8_t& g, uint8_t& b) {
        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
      });
}

template <class FileReader>
template <class AccessPixelFunctor>
auto Writer<FileReader>::WriteToRGB888(const PixelsSpec& pixels_spec,
                                       std::span<const uint8_t> pixels,
                                       const AccessPixelFunctor& access_pixel)
    -> bool {
  assert(format_spec_.num_bits_per_pixel == 24);

  const size_t width = format_spec_.width;
  const size_t pixels_height = pixels.size() / pixels_spec.row_stride;

  const int pixel_stride = pixels_spec.pixel_stride;
  const int row_stride = pixels_spec.row_stride;

  const bool is_stored_top_to_bottom = format_spec_.is_stored_top_to_bottom;

  const uint8_t* pixel_row =
      is_stored_top_to_bottom
          ? pixels.data()
          : pixels.data() + (pixels_height - 1) * row_stride;

  struct BGR {
    uint8_t b, g, r;
  };
  static_assert(sizeof(BGR) == 3);

  const size_t row_size_in_bytes =
      width * (format_spec_.num_bits_per_pixel / 8);
  const size_t num_bytes_to_pad_row = row_size_in_bytes_ - row_size_in_bytes;

  for (int32_t y = 0; y < pixels_height; ++y) {
    const uint8_t* pixel = pixel_row;
    for (int32_t x = 0; x < width; ++x, pixel += pixel_stride) {
      BGR bgr;

      assert(pixel >= &pixels.front());
      assert(pixel <= &pixels.back());

      access_pixel(pixel, bgr.r, bgr.g, bgr.b);

      if (!WriteObjectToFile(bgr)) {
        return false;
      }
    }

    // Write row padding.
    for (int i = 0; i < num_bytes_to_pad_row; ++i) {
      const uint8_t zero = 0;
      if (!WriteObjectToFile(zero)) {
        return false;
      }
    }

    if (is_stored_top_to_bottom) {
      pixel_row += row_stride;
    } else {
      pixel_row -= row_stride;
    }
  }

  return true;
}

template <class FileReader>
auto Writer<FileReader>::WriteToARGB888(const PixelsSpec& pixels_spec,
                                        std::span<const uint8_t> pixels)
    -> bool {
  if (pixels_spec.num_channels == 1) {
    return WriteToARGB888(pixels_spec,
                          pixels,
                          [](const uint8_t* pixel,
                             uint8_t& r,
                             uint8_t& g,
                             uint8_t& b,
                             uint8_t& a) {
                            r = pixel[0];
                            g = pixel[0];
                            b = pixel[0];
                            a = 255;
                          });
  }

  if (pixels_spec.num_channels == 2) {
    return WriteToARGB888(pixels_spec,
                          pixels,
                          [](const uint8_t* pixel,
                             uint8_t& r,
                             uint8_t& g,
                             uint8_t& b,
                             uint8_t& a) {
                            r = pixel[0];
                            g = pixel[1];
                            b = 0;
                            a = 255;
                          });
  }

  if (pixels_spec.num_channels == 3) {
    return WriteToARGB888(pixels_spec,
                          pixels,
                          [](const uint8_t* pixel,
                             uint8_t& r,
                             uint8_t& g,
                             uint8_t& b,
                             uint8_t& a) {
                            r = pixel[0];
                            g = pixel[1];
                            b = pixel[2];
                            a = 255;
                          });
  }

  return WriteToARGB888(
      pixels_spec,
      pixels,
      [](const uint8_t* pixel, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
        a = pixel[3];
      });
}

template <class FileReader>
template <class AccessPixelFunctor>
auto Writer<FileReader>::WriteToARGB888(const PixelsSpec& pixels_spec,
                                        std::span<const uint8_t> pixels,
                                        const AccessPixelFunctor& access_pixel)
    -> bool {
  assert(format_spec_.num_bits_per_pixel == 32);

  const size_t width = format_spec_.width;
  const size_t pixels_height = pixels.size() / pixels_spec.row_stride;

  const int pixel_stride = pixels_spec.pixel_stride;
  const int row_stride = pixels_spec.row_stride;

  const bool is_stored_top_to_bottom = format_spec_.is_stored_top_to_bottom;

  const uint8_t* pixel_row =
      is_stored_top_to_bottom
          ? pixels.data()
          : pixels.data() + (pixels_height - 1) * row_stride;

  struct BGRA {
    uint8_t b, g, r, a;
  };
  static_assert(sizeof(BGRA) == 4);

  const size_t row_size_in_bytes =
      width * (format_spec_.num_bits_per_pixel / 8);
  const size_t num_bytes_to_pad_row = row_size_in_bytes_ - row_size_in_bytes;

  for (int32_t y = 0; y < pixels_height; ++y) {
    const uint8_t* pixel = pixel_row;
    for (int32_t x = 0; x < width; ++x, pixel += pixel_stride) {
      BGRA bgra;

      assert(pixel >= &pixels.front());
      assert(pixel <= &pixels.back());

      access_pixel(pixel, bgra.r, bgra.g, bgra.b, bgra.a);

      if (!WriteObjectToFile(bgra)) {
        return false;
      }
    }

    // Write row padding.
    for (int i = 0; i < num_bytes_to_pad_row; ++i) {
      const uint8_t zero = 0;
      if (!WriteObjectToFile(zero)) {
        return false;
      }
    }

    if (is_stored_top_to_bottom) {
      pixel_row += row_stride;
    } else {
      pixel_row -= row_stride;
    }
  }

  return true;
}

}  // namespace TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE
}  // namespace TL_IMAGE_BMP_WRITER_NAMESPACE

#undef TL_IMAGE_BMP_WRITER_VERSION_MAJOR
#undef TL_IMAGE_BMP_WRITER_VERSION_MINOR
#undef TL_IMAGE_BMP_WRITER_VERSION_REVISION

#undef TL_IMAGE_BMP_WRITER_NAMESPACE

#undef TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE_CONCAT
#undef TL_IMAGE_BMP_WRITER_VERSION_NAMESPACE
