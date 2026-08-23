function clampU8(value) {
        return value < 0 ? 0 : value > 255 ? 255 : value;
}

export const PixelFormatId = {
        Unknown: 0,
        Mono8: 1,
        Mono10: 2,
        Mono12: 3,
        Mono14: 4,
        Mono16: 5,
        RG8: 6,
        RG10: 7,
        RG12: 8,
        RG14: 9,
        GR8: 10,
        GR10: 11,
        GR12: 12,
        GR14: 13,
        BG8: 14,
        BG10: 15,
        BG12: 16,
        BG14: 17,
        GB8: 18,
        GB10: 19,
        GB12: 20,
        GB14: 21,
        RG10P: 22,
        RG12P: 23,
        RG14P: 24,
        GR10P: 25,
        GR12P: 26,
        GR14P: 27,
        BG10P: 28,
        BG12P: 29,
        BG14P: 30,
        GB10P: 31,
        GB12P: 32,
        GB14P: 33,
        RGB888: 34,
        BGR888: 35,
        YUYV: 36,
        NV12: 37,
        Raw8: 38,
        Raw10: 39,
        Raw12: 40,
        Raw16: 41
};

export function isBayerRawFormat(formatId) {
        const id = Number(formatId);
        return id >= PixelFormatId.RG8 && id <= PixelFormatId.GB14P;
}

export function isGenericRawFormat(formatId) {
        const id = Number(formatId);
        return id >= PixelFormatId.Raw8 && id <= PixelFormatId.Raw16;
}

export function isMonoRawFormat(formatId) {
        const id = Number(formatId);
        return id >= PixelFormatId.Mono8 && id <= PixelFormatId.Mono16;
}

export function isRawFormat(formatId) {
        return isBayerRawFormat(formatId) || isGenericRawFormat(formatId) || isMonoRawFormat(formatId);
}

export function isColorRawFormat(formatId) {
        return isBayerRawFormat(formatId);
}

const BAYER_PATTERNS = {
        RG: 'RGGB',
        GR: 'GRBG',
        BG: 'BGGR',
        GB: 'GBRG'
};

export function parseBinaryFramePacket(arrayBuffer) {
        if (!arrayBuffer || arrayBuffer.byteLength < 48) return null;
        const view = new DataView(arrayBuffer);
        if (view.getUint32(0, true) !== 0x31464d43 || view.getUint16(4, true) !== 1) return null;
        const dataSize = view.getUint32(44, true);
        if (arrayBuffer.byteLength < 48 + dataSize) return null;
        return {
                meta: {
                        width: view.getUint32(8, true),
                        height: view.getUint32(12, true),
                        stride: view.getUint32(16, true),
                        formatId: view.getUint32(20, true),
                        bitsPerPixel: view.getUint16(24, true),
                        bitShift: view.getUint16(26, true),
                        sequence: Number((BigInt(view.getUint32(32, true)) << 32n) | BigInt(view.getUint32(28, true))),
                        timestampNs: Number((BigInt(view.getUint32(40, true)) << 32n) | BigInt(view.getUint32(36, true)))
                },
                bytes: new Uint8Array(arrayBuffer, 48, dataSize)
        };
}

export function formatLabel(formatId) {
        switch (Number(formatId)) {
                case PixelFormatId.Mono8: return 'GREY';
                case PixelFormatId.Mono10: return 'Y10';
                case PixelFormatId.Mono12: return 'Y12';
                case PixelFormatId.Mono14: return 'Y14';
                case PixelFormatId.Mono16: return 'Y16';
                case PixelFormatId.RG8: return 'RGGB';
                case PixelFormatId.RG10: return 'RG10';
                case PixelFormatId.RG12: return 'RG12';
                case PixelFormatId.RG14: return 'RG14';
                case PixelFormatId.GR8: return 'GRBG';
                case PixelFormatId.GR10: return 'BA10';
                case PixelFormatId.GR12: return 'BA12';
                case PixelFormatId.GR14: return 'BA14';
                case PixelFormatId.BG8: return 'BGGR';
                case PixelFormatId.BG10: return 'BG10';
                case PixelFormatId.BG12: return 'BG12';
                case PixelFormatId.BG14: return 'BG14';
                case PixelFormatId.GB8: return 'GBRG';
                case PixelFormatId.GB10: return 'GB10';
                case PixelFormatId.GB12: return 'GB12';
                case PixelFormatId.GB14: return 'GB14';
                case PixelFormatId.RG10P: return 'pRAA';
                case PixelFormatId.RG12P: return 'pRCC';
                case PixelFormatId.RG14P: return 'pREE';
                case PixelFormatId.GR10P: return 'pgAA';
                case PixelFormatId.GR12P: return 'pgCC';
                case PixelFormatId.GR14P: return 'pgEE';
                case PixelFormatId.BG10P: return 'pBAA';
                case PixelFormatId.BG12P: return 'pBCC';
                case PixelFormatId.BG14P: return 'pBEE';
                case PixelFormatId.GB10P: return 'pGAA';
                case PixelFormatId.GB12P: return 'pGCC';
                case PixelFormatId.GB14P: return 'pGEE';
                case PixelFormatId.RGB888: return 'RGB3';
                case PixelFormatId.BGR888: return 'BGR3';
                case PixelFormatId.YUYV: return 'YUYV';
                case PixelFormatId.NV12: return 'NV12';
                case PixelFormatId.Raw8: return 'GREY';
                case PixelFormatId.Raw10: return 'Y10';
                case PixelFormatId.Raw12: return 'Y12';
                case PixelFormatId.Raw16: return 'Y16';
                default: return `Format ${formatId}`;
        }
}

function makeMonoRgba(meta, bytes, shiftValue) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);
        for (let y = 0; y < meta.height; y += 1) {
                const rowStart = y * meta.stride;
                for (let x = 0; x < meta.width; x += 1) {
                        let sample = 0;
                        if (meta.bitsPerPixel > 8) {
                                const p = rowStart + x * 2;
                                const unpacked = (bytes[p] & 0xff) | ((bytes[p + 1] & 0xff) << 8);
                                sample = sampleToDisplayByte(unpacked, shiftValue, meta.bitsPerPixel);
                        } else {
                                sample = bytes[rowStart + x] & 0xff;
                        }
                        const out = (y * meta.width + x) * 4;
                        rgba[out] = sample;
                        rgba[out + 1] = sample;
                        rgba[out + 2] = sample;
                        rgba[out + 3] = 255;
                }
        }
        return rgba;
}

function rgb888ToRgba(meta, bytes) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);
        for (let y = 0; y < meta.height; y += 1) {
                const rowStart = y * meta.stride;
                for (let x = 0; x < meta.width; x += 1) {
                        const p = rowStart + x * 3;
                        const out = (y * meta.width + x) * 4;
                        rgba[out] = bytes[p];
                        rgba[out + 1] = bytes[p + 1];
                        rgba[out + 2] = bytes[p + 2];
                        rgba[out + 3] = 255;
                }
        }
        return rgba;
}

function bgr888ToRgba(meta, bytes) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);
        for (let y = 0; y < meta.height; y += 1) {
                const rowStart = y * meta.stride;
                for (let x = 0; x < meta.width; x += 1) {
                        const p = rowStart + x * 3;
                        const out = (y * meta.width + x) * 4;
                        rgba[out] = bytes[p + 2];
                        rgba[out + 1] = bytes[p + 1];
                        rgba[out + 2] = bytes[p];
                        rgba[out + 3] = 255;
                }
        }
        return rgba;
}

function writeYuvPixel(rgba, offset, y, u, v) {
        const d = u - 128;
        const e = v - 128;
        rgba[offset] = clampU8(y + 1.402 * e);
        rgba[offset + 1] = clampU8(y - 0.344136 * d - 0.714136 * e);
        rgba[offset + 2] = clampU8(y + 1.772 * d);
        rgba[offset + 3] = 255;
}

function yuyvToRgba(meta, bytes) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);
        for (let y = 0; y < meta.height; y += 1) {
                const rowStart = y * meta.stride;
                for (let x = 0; x < meta.width; x += 2) {
                        const p = rowStart + x * 2;
                        let out = (y * meta.width + x) * 4;
                        writeYuvPixel(rgba, out, bytes[p], bytes[p + 1], bytes[p + 3]);
                        writeYuvPixel(rgba, out + 4, bytes[p + 2], bytes[p + 1], bytes[p + 3]);
                }
        }
        return rgba;
}

function bayerFamilyFromFormatId(formatId) {
        const id = Number(formatId);

        if (id >= PixelFormatId.RG8 && id <= PixelFormatId.RG14) return 'RG';
        if (id >= PixelFormatId.GR8 && id <= PixelFormatId.GR14) return 'GR';
        if (id >= PixelFormatId.BG8 && id <= PixelFormatId.BG14) return 'BG';
        if (id >= PixelFormatId.GB8 && id <= PixelFormatId.GB14) return 'GB';

        if (id >= PixelFormatId.RG10P && id <= PixelFormatId.RG14P) return 'RG';
        if (id >= PixelFormatId.GR10P && id <= PixelFormatId.GR14P) return 'GR';
        if (id >= PixelFormatId.BG10P && id <= PixelFormatId.BG14P) return 'BG';
        if (id >= PixelFormatId.GB10P && id <= PixelFormatId.GB14P) return 'GB';

        return '';
}

function isPackedBayerFormat(formatId) {
        const id = Number(formatId);
        return id >= PixelFormatId.RG10P && id <= PixelFormatId.GB14P;
}

function packedGroupBytes(bitsPerPixel) {
        switch (Number(bitsPerPixel)) {
                case 10: return 5;
                case 12: return 3;
                case 14: return 7;
                default: return 0;
        }
}

function packedGroupPixels(bitsPerPixel) {
        return Number(bitsPerPixel) === 12 ? 2 : 4;
}

function packedRaw8Sample(meta, bytes, x, y) {
        const groupBytes = packedGroupBytes(meta.bitsPerPixel);
        const groupPixels = packedGroupPixels(meta.bitsPerPixel);
        const rowStart = y * meta.stride;
        const groupStart = rowStart + Math.floor(x / groupPixels) * groupBytes;
        return bytes[groupStart + (x % groupPixels)] & 0xff;
}

function unpackPackedRaw8(meta, bytes) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);
        for (let y = 0; y < meta.height; y += 1) {
                for (let x = 0; x < meta.width; x += 1) {
                        const sample = packedRaw8Sample(meta, bytes, x, y);
                        const out = (y * meta.width + x) * 4;
                        rgba[out] = sample;
                        rgba[out + 1] = sample;
                        rgba[out + 2] = sample;
                        rgba[out + 3] = 255;
                }
        }
        return rgba;
}

function sampleToDisplayByte(sample, shiftValue, bitsPerPixel) {
        const shift = Math.max(0, Math.min(8, Number(shiftValue) || 0));
        const shiftedSample = shift > 0 ? (sample >>> shift) : sample;
        const bits = Number(bitsPerPixel) || 8;

        // Match runtime conversion semantics for 10/12/14-bit raw data:
        // 1) apply configured right-shift, 2) reduce bit depth to 8-bit.
        if (bits > 8 && bits < 16) {
                const reductionShift = bits - 8;
                return (shiftedSample >>> reductionShift) & 0xff;
        }

        if (shift <= 0) {
                return sample & 0xff;
        }
        if (shift >= 8) {
                return (sample >> 8) & 0xff;
        }
        return (sample >> shift) & 0xff;
}

function rawSample8(meta, bytes, x, y, shiftValue) {
        const clampedX = Math.max(0, Math.min(meta.width - 1, x));
        const clampedY = Math.max(0, Math.min(meta.height - 1, y));
        const rowStart = clampedY * meta.stride;

        if (meta.bitsPerPixel > 8) {
                if (isPackedBayerFormat(meta.formatId)) {
                        return packedRaw8Sample(meta, bytes, clampedX, clampedY);
                }

                const p = rowStart + clampedX * 2;
                const sample = bytes[p] | (bytes[p + 1] << 8);
                return sampleToDisplayByte(sample, shiftValue, meta.bitsPerPixel);
        }

        return bytes[rowStart + clampedX] & 0xff;
}

function debayerToRgba(meta, bytes, shiftValue, pattern) {
        const rgba = new Uint8ClampedArray(meta.width * meta.height * 4);

        const firstRow = pattern[0] + pattern[1];
        const secondRow = pattern[2] + pattern[3];

        for (let y = 0; y < meta.height; y += 1) {
                const rowPattern = (y & 1) === 0 ? firstRow : secondRow;
                for (let x = 0; x < meta.width; x += 1) {
                        const site = (x & 1) === 0 ? rowPattern[0] : rowPattern[1];

                        let r = 0;
                        let g = 0;
                        let b = 0;

                        if (site === 'R') {
                                // R site
                                r = rawSample8(meta, bytes, x, y, shiftValue);
                                g = (rawSample8(meta, bytes, x - 1, y, shiftValue) + rawSample8(meta, bytes, x + 1, y, shiftValue) + rawSample8(meta, bytes, x, y - 1, shiftValue) + rawSample8(meta, bytes, x, y + 1, shiftValue) + 2) >> 2;
                                b = (rawSample8(meta, bytes, x - 1, y - 1, shiftValue) + rawSample8(meta, bytes, x + 1, y - 1, shiftValue) + rawSample8(meta, bytes, x - 1, y + 1, shiftValue) + rawSample8(meta, bytes, x + 1, y + 1, shiftValue) + 2) >> 2;
                        } else if (site === 'B') {
                                // B site
                                b = rawSample8(meta, bytes, x, y, shiftValue);
                                g = (rawSample8(meta, bytes, x - 1, y, shiftValue) + rawSample8(meta, bytes, x + 1, y, shiftValue) + rawSample8(meta, bytes, x, y - 1, shiftValue) + rawSample8(meta, bytes, x, y + 1, shiftValue) + 2) >> 2;
                                r = (rawSample8(meta, bytes, x - 1, y - 1, shiftValue) + rawSample8(meta, bytes, x + 1, y - 1, shiftValue) + rawSample8(meta, bytes, x - 1, y + 1, shiftValue) + rawSample8(meta, bytes, x + 1, y + 1, shiftValue) + 2) >> 2;
                        } else {
                                // G site. Interpolation direction depends on neighboring colors.
                                g = rawSample8(meta, bytes, x, y, shiftValue);

                                const leftSite = (x & 1) === 0 ? rowPattern[1] : rowPattern[0];
                                const horizontalIsRed = leftSite === 'R';

                                if (horizontalIsRed) {
                                        r = (rawSample8(meta, bytes, x - 1, y, shiftValue) + rawSample8(meta, bytes, x + 1, y, shiftValue) + 1) >> 1;
                                        b = (rawSample8(meta, bytes, x, y - 1, shiftValue) + rawSample8(meta, bytes, x, y + 1, shiftValue) + 1) >> 1;
                                } else {
                                        r = (rawSample8(meta, bytes, x, y - 1, shiftValue) + rawSample8(meta, bytes, x, y + 1, shiftValue) + 1) >> 1;
                                        b = (rawSample8(meta, bytes, x - 1, y, shiftValue) + rawSample8(meta, bytes, x + 1, y, shiftValue) + 1) >> 1;
                                }
                        }

                        const out = (y * meta.width + x) * 4;
                        rgba[out] = r;
                        rgba[out + 1] = g;
                        rgba[out + 2] = b;
                        rgba[out + 3] = 255;
                }
        }

        return rgba;
}

function bayerPatternFromFormatId(formatId) {
        const family = bayerFamilyFromFormatId(formatId);
        return family ? BAYER_PATTERNS[family] : '';
}

export function renderPacketToRgba(meta, bytes, debayerEnabled = false) {
        const bayerPattern = bayerPatternFromFormatId(meta.formatId);
        const canDebayer = Boolean(bayerPattern);
        const shiftValue = meta.bitShift;

        if (debayerEnabled && canDebayer) {
                return debayerToRgba(meta, bytes, shiftValue, bayerPattern);
        }

        if (meta.formatId === PixelFormatId.RGB888) return rgb888ToRgba(meta, bytes);
        if (meta.formatId === PixelFormatId.BGR888) return bgr888ToRgba(meta, bytes);
        if (meta.formatId === PixelFormatId.YUYV) return yuyvToRgba(meta, bytes);
        if (isPackedBayerFormat(meta.formatId)) return unpackPackedRaw8(meta, bytes);

        return makeMonoRgba(meta, bytes, shiftValue);
}
