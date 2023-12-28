// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Simple implementation of BMP reader which does not use memory allocation and
// uses a custom IO provider to perform disk IO.
//
//
// Example
// =======
//
//   File bmp_file;
//   <open the file>
//
//   Reader<File> reader;
//   reader.Open(bmp_file);
//
//   const PixelsSpec pixels_spec = {
//       .num_channels = 3,
//   };
//
//   auto pixels = <allocate pixels>;
//
//   reader.Read(pixels_spec, pixels);
//
//
// File Reader
// ===========
//
// The file reader is to implement a Read() method which can be invoked with the
// following signature:
//
//   auto Read(void* ptr, SizeType num_bytes_to_read) -> IntType;
//
// It is possible to have trailing arguments with default values in the
// method if they do not change the expected invoke arguments.
//
// The num_bytes_to_read denotes the number of bytes which the BMP reader
// expects to be read from the file.
//
// Upon success the Read() is to return the number of bytes actually read.
//
// The BMP reader expects the number of actually read bytes to match the number
// of requested bytes to read. This gives flexibility to the error handling in
// the file reader implementation: both read() and fread() style of the return
// value in the case of error or reading past the EOF will work for the BMP
// reader.
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
#include <cstring>
#include <span>

// Semantic version of the tl_image_bmp_reader library.
#define TL_IMAGE_BMP_READER_VERSION_MAJOR 0
#define TL_IMAGE_BMP_READER_VERSION_MINOR 0
#define TL_IMAGE_BMP_READER_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_IMAGE_BMP_READER_NAMESPACE
#  define TL_IMAGE_BMP_READER_NAMESPACE tiny_lib::image_bmp_reader
#endif

// Helpers for TL_IMAGE_BMP_READER_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)     \
  v_##id1##_##id2##_##id3
#define TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT(id1, id2, id3)            \
  TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_IMAGE_BMP_READER_VERSION_NAMESPACE -> v_0_1_9
#define TL_IMAGE_BMP_READER_VERSION_NAMESPACE                                  \
  TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT(                                \
      TL_IMAGE_BMP_READER_VERSION_MAJOR,                                       \
      TL_IMAGE_BMP_READER_VERSION_MINOR,                                       \
      TL_IMAGE_BMP_READER_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_IMAGE_BMP_READER_NAMESPACE {
inline namespace TL_IMAGE_BMP_READER_VERSION_NAMESPACE {

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

  // Returns true if the pixels of this specification are known how to be read.
  inline auto IsValid() const -> bool {
    return width > 0 && height > 0 && GetNumChannels() != 0;
  }

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
  // At the maximum this number of channels will be read from file and written
  // to the memory. When the number of channels in memory is less than in the
  // file the extra channels are ignored upon read. When it is higher than the
  // number of channels in the file the extra channels are written as maximum
  // value for the channel type.
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

// Detailed result code of reading.
enum class Result {
  // Reading succeeded.
  kOk,

  // No pixels were read at all.
  kUnavailable,

  // The file has been only partially read.
  // Possibly due to an abort during file saving.
  kPartial,

  // Pixels array of an insufficient size has been provided.
  kResourceExhausted,
};

template <class FileReader>
class Reader {
 public:
  Reader() = default;

  Reader(Reader&& other) noexcept = default;
  auto operator=(Reader&& other) -> Reader& = default;

  // Delete copy constructor and assignment operator.
  //
  // Simplifies the FileReader API by avoiding requirement to implement the
  // copy file descriptor API call.
  Reader(const Reader& other) noexcept = delete;
  auto operator=(const Reader& other) -> Reader& = delete;

  ~Reader() = default;

  // Open the file for reading using given file reader.
  //
  // The file reader is referenced by the BMP reader and caller is to guarantee
  // it is valid throughout the BMP file open and pixels reading.
  //
  // Performs check for BMP header and verifies that the pixels can be read
  // from the file. Namely that the pixels are stored in a supported format.
  //
  // The underlying file reader is supposed to be open and ready to be read, as
  // well as positioned at the beginning of the BMP file.
  //
  // Returns true if pixels are ready to be read.
  //
  // Multiple calls to Open() is undefined.
  auto Open(FileReader& file_reader) -> bool;

  // Close the file reader.
  auto Close() -> bool;

  // Get format specification of the file.
  //
  // Requires BMP file to be successfully open for read first and will have an
  // undefined behavior if the Open() returned false.
  inline auto GetFormatSpec() const -> const FormatSpec&;

  // Read pixels from file into the given memory of the given layout
  // specification.
  //
  // Returns Result::kOk on success.
  auto Read(const PixelsSpec& pixels_spec, std::span<uint8_t> pixels) -> Result;

 private:
  // Read object from file stream directly into memory, without endian
  // conversion or any other semantic checks.
  //
  // Returns true if the object is fully read, false otherwise.
  template <class T>
  inline auto ReadObjectInMemory(T& object) -> bool;

  // Convert byte order from the one stored in the file to the native to the
  // current platform.
  template <class T>
  inline void FileToNativeEndian(T& object);

  // Read object from the file stream into memory and make its endian native to
  // the current platform.
  template <class T>
  inline auto ReadObjectInNativeMemory(T& object) -> bool;

  // Skip given number of bytes from the current location of file.
  // Returns true if all requested bytes has been skipped.
  inline auto FileSkipNumBytes(uint32_t num_bytes) -> bool;

  // Read the header of the BMP file and store it in the internal state.
  inline auto ReadHeader() -> bool;

  // Re-position the file to the beginning of the pixels array.
  inline auto SeekPixelArray() -> bool;

  // Resolve the storage specification to the actual numbers.
  inline auto ResolvePixelsSpec(const PixelsSpec& pixels_spec) -> PixelsSpec;

  // Readers of single channel value.
  // Returns true on success.
  inline auto ReadSingleColorChannel(uint8_t& color) -> bool;

  // Read pixels from RGB888 file into the pixels of the given specification.
  auto ReadFromRGB888(const PixelsSpec& pixels_spec, std::span<uint8_t> pixels)
      -> Result;

  // An implementation of above with a given pixel value assignment functor.
  // Allows to move channels count check to the outer loop.
  template <class AssignPixelFunctor>
  auto ReadFromRGB888(const PixelsSpec& pixels_spec,
                      std::span<uint8_t> pixels,
                      const AssignPixelFunctor& assign_pixel) -> Result;

  FileReader* file_reader_{nullptr};

  // Endian in which multi-byte values are stored in the file.
  std::endian file_endian_{std::endian::little};

  bool is_open_attempted_{false};  // True if Open() has been called.
  bool is_open_{false};            // True if the Open() returned true.

  FormatSpec format_spec_;

  uint32_t offset_to_pixel_array_{0};

  // Number of bytes used to store the row of pixels in the file.
  // The BMP format rounds up storage size of rows to 4 bytes.
  uint32_t row_size_in_bytes_{0};
};

// Check whether the memory in `header` could be a BMP file.
// The check is done based on signatures in the header.
//
// Returns true if the header is detected to be a plausible BMP header.
inline auto IsBMPHeader(std::span<const uint8_t> header) -> bool;

////////////////////////////////////////////////////////////////////////////////
// Internal helpers for public API implementation.

namespace internal {

// Simple implementation if a reader API from memory.
class MemoryReader {
 public:
  explicit MemoryReader(const std::span<const uint8_t> storage)
      : storage_(storage) {}

  inline auto Read(void* buffer, size_t num_bytes_to_read) -> size_t {
    if (num_bytes_to_read == 0) {
      return 0;
    }

    const size_t storage_size = storage_.size();
    if (position_ >= storage_size) {
      return 0;
    }

    const size_t num_actual_bytes_to_read =
        std::min(num_bytes_to_read, storage_size - position_);

    memcpy(buffer, storage_.data() + position_, num_actual_bytes_to_read);

    position_ += num_actual_bytes_to_read;

    return num_actual_bytes_to_read;
  }

 private:
  std::span<const uint8_t> storage_;
  size_t position_ = 0;
};

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// Public API implementation.

template <class FileReader>
auto Reader<FileReader>::Open(FileReader& file_reader) -> bool {
  assert(!is_open_attempted_);

  is_open_attempted_ = true;

  file_reader_ = &file_reader;
  is_open_ = ReadHeader();

  return is_open_;
}

template <class FileReader>
auto Reader<FileReader>::Close() -> bool {
  if (!is_open_attempted_) {
    return false;
  }

  is_open_attempted_ = false;
  file_reader_ = nullptr;
  is_open_ = false;

  return true;
}

template <class FileReader>
inline auto Reader<FileReader>::GetFormatSpec() const -> const FormatSpec& {
  assert(is_open_);

  return format_spec_;
}

template <class FileReader>
auto Reader<FileReader>::Read(const PixelsSpec& pixels_spec,
                              std::span<uint8_t> pixels) -> Result {
  assert(is_open_);

  const PixelsSpec actual_pixels_spec = ResolvePixelsSpec(pixels_spec);

  switch (format_spec_.num_bits_per_pixel) {
    case 24: return ReadFromRGB888(actual_pixels_spec, pixels);
  }

  // Unknown or unsupported bit depth.
  return Result::kUnavailable;
}

inline auto IsBMPHeader(std::span<const uint8_t> header) -> bool {
  internal::MemoryReader memory_reader(header);
  Reader<internal::MemoryReader> reader;

  return reader.Open(memory_reader);
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

template <class FileReader>
template <class T>
inline auto Reader<FileReader>::ReadObjectInMemory(T& object) -> bool {
  void* object_ptr = static_cast<void*>(&object);
  constexpr size_t kObjectSize = sizeof(T);

  return file_reader_->Read(object_ptr, kObjectSize) == kObjectSize;
}

template <class FileReader>
template <class T>
inline void Reader<FileReader>::FileToNativeEndian(T& object) {
  if (std::endian::native != file_endian_) {
    object = Byteswap(object);
  }
}

template <class FileReader>
template <class T>
inline auto Reader<FileReader>::ReadObjectInNativeMemory(T& object) -> bool {
  if (!ReadObjectInMemory(object)) {
    return false;
  }

  FileToNativeEndian(object);

  return true;
}

template <class FileReader>
inline auto Reader<FileReader>::FileSkipNumBytes(const uint32_t num_bytes)
    -> bool {
  uint32_t num_remaining_bytes_to_skip = num_bytes;

  // Read in chunks of 4 bytes to minimize the number of reads.
  while (num_remaining_bytes_to_skip >= 4) {
    uint32_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 4) != 4) {
      return false;
    }
    num_remaining_bytes_to_skip -= 4;
  }

  // Read remaining bytes.
  for (uint32_t i = 0; i < num_remaining_bytes_to_skip; ++i) {
    uint8_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 1) != 1) {
      return false;
    }
  }

  return true;
}

template <class FileReader>
auto Reader<FileReader>::ReadHeader() -> bool {
  internal::FileHeader file_header;
  internal::InfoHeader info_header;

  if (!ReadObjectInNativeMemory(file_header)) {
    return false;
  }

  // Check whether this is recognizable type. If not there is no reason to
  // keep reading.
  if (!internal::FileHeaderType::IsKnown(file_header.type)) {
    return false;
  }

  if (!ReadObjectInNativeMemory(info_header)) {
    return false;
  }

  if (info_header.size < sizeof(internal::InfoHeader)) {
    // Either invalid BMP file or the file header is too old.
    return false;
  }

  if (info_header.compression != internal::Compression::BI_RGB) {
    // Compressed BMP images are not supported.
    return false;
  }

  // Store settings needed later for decoding pixels array.
  offset_to_pixel_array_ = file_header.offset_to_pixel_array;

  // Store the format specification.
  format_spec_.width = info_header.width;
  format_spec_.height = info_header.height;
  format_spec_.num_bits_per_pixel = info_header.num_bits_per_pixel;

  if (format_spec_.height < 0) {
    format_spec_.is_stored_top_to_bottom = true;
    format_spec_.height = -format_spec_.height;
  } else {
    format_spec_.is_stored_top_to_bottom = false;
  }

  // Calculate the row size with its padding.
  row_size_in_bytes_ =
      (info_header.num_bits_per_pixel * info_header.width + 31) / 32 * 4;

  return format_spec_.IsValid();
}

template <class FileReader>
inline auto Reader<FileReader>::SeekPixelArray() -> bool {
  const size_t header_size =
      sizeof(internal::FileHeader) + sizeof(internal::InfoHeader);

  if (header_size > offset_to_pixel_array_) {
    return false;
  }

  const uint32_t num_bytes_to_skip =
      offset_to_pixel_array_ - uint32_t(header_size);

  return FileSkipNumBytes(num_bytes_to_skip);
}

template <class FileReader>
inline auto Reader<FileReader>::ResolvePixelsSpec(const PixelsSpec& pixels_spec)
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
inline auto Reader<FileReader>::ReadSingleColorChannel(uint8_t& color) -> bool {
  return ReadObjectInMemory(color);
}

template <class FileReader>
auto Reader<FileReader>::ReadFromRGB888(const PixelsSpec& pixels_spec,
                                        std::span<uint8_t> pixels) -> Result {
  if (pixels_spec.num_channels == 1) {
    return ReadFromRGB888(
        pixels_spec,
        pixels,
        [](uint8_t* pixel, uint8_t r, uint8_t /*g*/, uint8_t /*b*/) {
          pixel[0] = r;
        });
  }

  if (pixels_spec.num_channels == 2) {
    return ReadFromRGB888(
        pixels_spec,
        pixels,
        [](uint8_t* pixel, uint8_t r, uint8_t g, uint8_t /*b*/) {
          pixel[0] = r;
          pixel[1] = g;
        });
  }

  if (pixels_spec.num_channels == 3) {
    return ReadFromRGB888(pixels_spec,
                          pixels,
                          [](uint8_t* pixel, uint8_t r, uint8_t g, uint8_t b) {
                            pixel[0] = r;
                            pixel[1] = g;
                            pixel[2] = b;
                          });
  }

  return ReadFromRGB888(
      pixels_spec, pixels, [](uint8_t* pixel, uint8_t r, uint8_t g, uint8_t b) {
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
        pixel[2] = 255;
      });
}

template <class FileReader>
template <class AssignPixelFunctor>
auto Reader<FileReader>::ReadFromRGB888(const PixelsSpec& pixels_spec,
                                        std::span<uint8_t> pixels,
                                        const AssignPixelFunctor& assign_pixel)
    -> Result {
  assert(format_spec_.num_bits_per_pixel == 24);

  if (!SeekPixelArray()) {
    return Result::kUnavailable;
  }

  const size_t width = format_spec_.width;
  const size_t height = format_spec_.height;

  const size_t pixel_stride = pixels_spec.pixel_stride;
  const size_t row_stride = pixels_spec.row_stride;

  const size_t num_required_elements =
      row_stride * (height - 1) + pixel_stride * width;
  if (pixels.size() < num_required_elements) {
    return Result::kResourceExhausted;
  }

  const bool is_stored_top_to_bottom = format_spec_.is_stored_top_to_bottom;

  uint8_t* pixel_row = is_stored_top_to_bottom
                           ? pixels.data()
                           : pixels.data() + (height - 1) * row_stride;

  const size_t row_size_in_bytes =
      width * (format_spec_.num_bits_per_pixel / 8);
  const size_t num_bytes_to_pad_row = row_size_in_bytes_ - row_size_in_bytes;

  for (int32_t y = 0; y < height; ++y) {
    uint8_t* pixel = pixel_row;
    for (int32_t x = 0; x < width; ++x, pixel += pixel_stride) {
      uint8_t r, g, b;  // NOLINT(readability-isolate-declaration)
      if (!ReadSingleColorChannel(b) || !ReadSingleColorChannel(g) ||
          !ReadSingleColorChannel(r)) {
        if (x != 0 || y != 0) {
          return Result::kPartial;
        }
        return Result::kUnavailable;
      }

      assert(pixel >= &pixels.front());
      assert(pixel <= &pixels.back());

      assign_pixel(pixel, r, g, b);
    }

    FileSkipNumBytes(num_bytes_to_pad_row);

    if (is_stored_top_to_bottom) {
      pixel_row += row_stride;
    } else {
      pixel_row -= row_stride;
    }
  }

  return Result::kOk;
}

}  // namespace TL_IMAGE_BMP_READER_VERSION_NAMESPACE
}  // namespace TL_IMAGE_BMP_READER_NAMESPACE

#undef TL_IMAGE_BMP_READER_VERSION_MAJOR
#undef TL_IMAGE_BMP_READER_VERSION_MINOR
#undef TL_IMAGE_BMP_READER_VERSION_REVISION

#undef TL_IMAGE_BMP_READER_NAMESPACE

#undef TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_IMAGE_BMP_READER_VERSION_NAMESPACE_CONCAT
#undef TL_IMAGE_BMP_READER_VERSION_NAMESPACE
