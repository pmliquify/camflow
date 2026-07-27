# Image Conversion

CamFlow separates image representation from image conversion.

## ImageBuffer

`ImageBuffer` is the runtime image container. It is OpenCV-free and can be used by `V4L2Source`, `TCPSink` and the runtime core.

## Converter registry

The runtime owns an `ImageConverterRegistry`. Converters are selected by supported source and destination formats.

## ImageConverter

`ImageConverter` is the project's concrete converter implementation and uses OpenCV.

OpenCV-dependent nodes may use it:

- `FileSource`
- `FileSink`
- `CompositorProcessor`
- `DebayerProcessor`
- `CCMProcessor`

## Late conversion strategy

CamFlow now applies format conversion as late as possible:

- `FileSource` keeps RAW payloads unchanged in `FrameContext`.
- `FileSink` converts only when output format requires it (for example RAW -> JPG).
- `CCMProcessor` and `FileSink` automatically use the standard debayer path when Bayer RAW input must be converted to color.
- `ImageConverter` supports YUYV and NV12 both as input formats and as raw output targets.
- For RAW Bayer/mono inputs, `ImageBuffer::bitShift` is applied before automatic debayer/RAW conversion by dividing all pixel values by `2^bitShift` in OpenCV.
- RAW14 is supported for mono (`Y14`) and Bayer (`RG14/GB14/GR14/BG14` plus packed `*14P`) in FileSource/FileSink and converter paths.

Supported YUV roundtrips:

- Encoded image -> `YUYV` raw file -> encoded image
- Encoded image -> `NV12` raw file -> encoded image
- `YUYV` raw file -> `BGR888` / `RGB888` / `Mono8`
- `NV12` raw file -> `BGR888` / `RGB888` / `Mono8`

For raw `YUYV` and `NV12` files, `FileSource` still requires width and height unless they can be parsed from the filename. Stride is derived automatically when omitted.

## Platform converters

Future converters may support CUDA, V4L2 M2M, ISP blocks or other platform-specific hardware accelerators.
