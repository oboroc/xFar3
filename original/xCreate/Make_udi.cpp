#include <windows.h>
#include "trdos.hpp"
#include "make.hpp"

WORD wdcrc(BYTE *ptr, WORD size, BYTE dataId)
{
   DWORD crc1 = 0xCDB4;
   int j;
   crc1 ^= dataId << 8;
   for(j = 8; j; j--)
      if((crc1 *= 2l) & 0x10000)
        crc1 ^= 0x1021; // bit representation of x^12+x^5+1
   while(size--)
   {
     crc1 ^= (*ptr++) << 8;
     for(j = 8; j; j--)
        if((crc1 *= 2l) & 0x10000)
          crc1 ^= 0x1021; // bit representation of x^12+x^5+1
   }
   return (WORD) (((crc1 & 0xFF00) >> 8) | ((crc1 & 0xFF) << 8));
}

bool createUDI(Track0 track0, int totalSecs, BYTE *interleave, HANDLE image, HANDLE boot, char *comment)
{
  int i;
  const WORD emptySecCRC = 0x22E1;

  DWORD noBytesWritten;
  DWORD noBytesRead;
  DWORD fileSize = 0x112CF0;

  BYTE emptySec[secSize];
  ZeroMemory(emptySec, secSize);

  BYTE GAP[192];
  FillMemory(GAP, sizeof(GAP), 0x4E);

  BYTE sinc[12];
  ZeroMemory(sinc, sizeof(sinc));

  BYTE CLC[782];
  ZeroMemory(CLC, sizeof(CLC));
  for(i = 0; i < noSecs/4; ++i)
  {
    CLC[  2+i*189] = 0xC0;
    CLC[  3+i*189] = 0x01;
    CLC[  8+i*189] = 0x1C;
    CLC[ 50+i*189] = 0x07;
    CLC[ 55+i*189] = 0x70;
    CLC[ 97+i*189] = 0x1C;
    CLC[102+i*189] = 0xC0;
    CLC[103+i*189] = 0x01;
    CLC[144+i*189] = 0x70;
    CLC[150+i*189] = 0x07;
  }

  //заголовок файла
  UDIHdr hdr =
  {
    'U', 'D', 'I', '!',
    0x00l,                // размер файла - 4
    0x00,                 // версия формата
    0x4F,                 // Максимально доступный цилиндр
    0x01,                 // Максимальный номер поверхности диска
    0x00,                 // не используется поэтому всегда 0x00
    0x00,0x00,0x00,0x00   // Длина дополнительного заголовка
  };
  if(comment)
  {
    fileSize += lstrlen(comment)+1;
    hdr.flag = 1;
  }
  hdr.fileSize = fileSize;
  WriteFile(image, &hdr, sizeof(UDIHdr), &noBytesWritten, NULL);

  //дорожки
  int savedSecs = 0;
  for(i = 0; i < 2*noTrks; ++i)
  {
    BYTE trkHdr[] = { 0, 0x6A, 0x18 };
    WriteFile(image, trkHdr, sizeof(trkHdr), &noBytesWritten, NULL);
    WriteFile(image, GAP, GAP1Size, &noBytesWritten, NULL);

    BYTE buf[secSize*noSecs];
    
    if((savedSecs < totalSecs) && (i!=0))
    {
      if((totalSecs - savedSecs) < 16)
      {
        ZeroMemory(buf, noSecs*secSize);
        ReadFile(boot, buf, (totalSecs - savedSecs)*secSize, &noBytesRead, NULL);
      }
      else
        ReadFile(boot, buf, noSecs*secSize, &noBytesRead, NULL);
    }

    for(int j = 0; j < noSecs; ++j)
    {
      WriteFile(image, sinc, sizeof(sinc), &noBytesWritten, NULL);
      BYTE AM[] = { 0xA1, 0xA1, 0xA1, 0 };
      AM[3] = 0xFE;
      WriteFile(image, AM, sizeof(AM), &noBytesWritten, NULL);

      BYTE secHdr[] = { 0, 0, 0, 1 };
      secHdr[0] = i/2;
      secHdr[2] = interleave[j];
      WORD CRC;
      CRC = wdcrc(secHdr, sizeof(secHdr), 0xFE);
      WriteFile(image, secHdr, sizeof(secHdr), &noBytesWritten, NULL);
      WriteFile(image, &CRC, sizeof(CRC), &noBytesWritten, NULL);
      WriteFile(image, GAP, GAP2Size, &noBytesWritten, NULL);
      WriteFile(image, sinc, sizeof(sinc), &noBytesWritten, NULL);
      AM[3] = 0xFB;
      WriteFile(image, AM, sizeof(AM), &noBytesWritten, NULL);

      if(i == 0)
      {
        BYTE *ptr = (BYTE*)&track0 + secSize * (interleave[j] - 1);
        WriteFile(image, ptr, secSize, &noBytesWritten, NULL);
        CRC = wdcrc(ptr, secSize, 0xFB);
        WriteFile(image, &CRC, sizeof(CRC), &noBytesWritten, NULL);
        WriteFile(image, GAP, GAP3Size, &noBytesWritten, NULL);
        continue;
      }

      if(savedSecs < totalSecs)
      {
        CRC = wdcrc(buf + secSize * (interleave[j] - 1), secSize, 0xFB);
        WriteFile(image, buf + secSize * (interleave[j] - 1), secSize, &noBytesWritten, NULL);
        WriteFile(image, &CRC, sizeof(CRC), &noBytesWritten, NULL);
        if(((totalSecs - savedSecs) >= 16) || (((totalSecs - savedSecs) < 16) && (interleave[j] <= (totalSecs%16))))
          savedSecs++;
      }
      else
      {
        WriteFile(image, emptySec, secSize, &noBytesWritten, NULL);
        WriteFile(image, &emptySecCRC, sizeof(emptySecCRC), &noBytesWritten, NULL);
      }

      WriteFile(image, GAP, GAP3Size, &noBytesWritten, NULL);
    }

    WriteFile(image, GAP, GAP4Size, &noBytesWritten, NULL);
    WriteFile(image, CLC, sizeof(CLC), &noBytesWritten, NULL);
  }

  if(comment)
    WriteFile(image, comment, lstrlen(comment)+1, &noBytesWritten, NULL);

  long CRC32 = 0xFFFFFFFF;

  HANDLE mh = CreateFileMapping(image, 0, PAGE_READONLY, 0, 0, NULL);
  if(mh != NULL)
  {
    void *buf = MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0);

    for (DWORD i = 0; i < fileSize; i++)
    {
      DWORD temp;
      CRC32 ^= -1 ^ *(((BYTE*)buf)+i);
      for(BYTE k = 8; k--;)
        { temp = -(CRC32 & 1); CRC32 >>= 1; CRC32 ^= 0xEDB88320 & temp; }
      CRC32 ^= -1;
    }

    UnmapViewOfFile(buf);
    CloseHandle(mh);
  }

  WriteFile(image, &CRC32, sizeof(CRC32), &noBytesWritten, NULL);

  return true;
}
