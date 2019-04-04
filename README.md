OpenGaleFile
========================
### GaleFile2: The Revenge of GaleFile

OpenGaleFile is a library for loading up the GAL file format from the Graphics Gale program (found at [https://graphicsgale.com/us/](https://graphicsgale.com/us/)), which is primarily focused at creating and animated pixel art.

Since the provided DLL by the creators of Graphics Gale is a Windows-only 32-bit library, the platforms where GALs can be directly read is terribly limited without roundabout solutions.

## The GAL Format

Essentially is an 8-byte magic string, followed by any number of 4-byte integer + Gzipped buffer pairs. For each pair, the 4-byte integer refers to the length of the compressed buffer that follows it.

The first buffer is always a Gzipped XML file that gives human-readable information about the image that is stored, which includes:
* Background color
* Layer information
* Frame information
* Image dimensions
* Bit depth
* Palette (if 8 bit or less)
* And much more!
