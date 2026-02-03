#ifndef FILEIO_H
#define FILEIO_H

// File reading
WCHAR* FileIO_ReadFile(const WCHAR* path, TextEncoding* detectedEncoding);
BOOL FileIO_WriteFile(const WCHAR* path, const WCHAR* content, TextEncoding encoding);

// Encoding detection
TextEncoding FileIO_DetectEncoding(const BYTE* data, size_t size);
const WCHAR* FileIO_GetEncodingName(TextEncoding encoding);

// BOM handling
size_t FileIO_GetBOMSize(TextEncoding encoding);
void FileIO_WriteBOM(HANDLE hFile, TextEncoding encoding);

#endif // FILEIO_H
