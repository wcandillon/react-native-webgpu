/* eslint-disable prefer-destructuring */
// HDR format loading from https://enkimute.github.io/hdrpng.js/

export function loadHDR(data: Uint8Array): {
  rgbe: Uint8Array;
  width: number;
  height: number;
} {
  let header = "";
  let pos = 0;

  // Read header
  while (!header.match(/\n\n[^\n]+\n/g)) {
    header += String.fromCharCode(data[pos++]);
  }

  // Check format
  const [, format] = header.match(/FORMAT=(.*)$/m)!;
  if (format !== "32-bit_rle_rgbe") {
    console.warn("unknown format : " + format);
  }

  // Parse resolution
  const rez = header.split(/\n/).reverse()[1].split(" ");
  const width = parseInt(rez[3], 10);
  const height = parseInt(rez[1], 10);

  // Create image
  const img = new Uint8Array(width * height * 4);
  let ipos = 0;

  // Read all scanlines
  for (let j = 0; j < height; j++) {
    const rgbe = data.slice(pos, (pos += 4));
    const scanline: number[] = [];

    if (rgbe[0] !== 2 || rgbe[1] !== 2 || rgbe[2] & 0x80) {
      let len = width;
      let rs = 0;
      pos -= 4;

      while (len > 0) {
        img.set(data.slice(pos, (pos += 4)), ipos);
        if (img[ipos] === 1 && img[ipos + 1] === 1 && img[ipos + 2] === 1) {
          let i = img[ipos + 3] << rs;
          while (i > 0) {
            img.set(img.slice(ipos - 4, ipos), ipos);
            ipos += 4;
            len--;
            i--;
          }
          rs += 8;
        } else {
          len--;
          ipos += 4;
          rs = 0;
        }
      }
    } else {
      if ((rgbe[2] << 8) + rgbe[3] !== width) {
        console.warn("HDR line mismatch ..");
      }

      for (let i = 0; i < 4; i++) {
        let ptr = i * width;
        const ptrEnd = (i + 1) * width;
        let buf: Uint8Array;
        let count: number;

        while (ptr < ptrEnd) {
          buf = data.slice(pos, (pos += 2));
          if (buf[0] > 128) {
            count = buf[0] - 128;
            while (count-- > 0) {
              scanline[ptr++] = buf[1];
            }
          } else {
            count = buf[0] - 1;
            scanline[ptr++] = buf[1];
            while (count-- > 0) {
              scanline[ptr++] = data[pos++];
            }
          }
        }
      }

      for (let i = 0; i < width; i++) {
        img[ipos++] = scanline[i];
        img[ipos++] = scanline[i + width];
        img[ipos++] = scanline[i + 2 * width];
        img[ipos++] = scanline[i + 3 * width];
      }
    }
  }

  return { rgbe: img, width, height };
}
