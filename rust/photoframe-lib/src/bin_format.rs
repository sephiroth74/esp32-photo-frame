use anyhow::{anyhow, Result};
use crc32fast::Hasher as Crc32Hasher;

/// Magic 'PFR1' little-endian (0x50465231)
pub const BIN_MAGIC: u32 = 0x5046_5231;
/// Header size: magic(4) + ver(1) + hlen(2) + w(2) + h(2) + rot(1) + color(1) + payload_len(4) + header_crc32(4)
pub const BIN_HEADER_SIZE: usize = 21;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C, packed)]
pub struct BinHeader {
    pub version: u8,
    pub header_len: u16,
    pub width: u16,
    pub height: u16,
    pub rotation: u8,
    pub color_mode: u8, // 0 = BW, 1 = 6C
    pub payload_len: u32,
    pub header_crc32: u32,
}

/// Build full .bin file bytes: header + payload + payload_crc32
pub fn build_bin_file(
    payload: &[u8],
    width: u16,
    height: u16,
    rotation: u8,
    color_mode: u8,
    version: u8,
) -> Vec<u8> {
    let mut header = Vec::with_capacity(BIN_HEADER_SIZE);
    header.extend_from_slice(&BIN_MAGIC.to_le_bytes());
    header.push(version);
    header.extend_from_slice(&(BIN_HEADER_SIZE as u16).to_le_bytes());
    header.extend_from_slice(&width.to_le_bytes());
    header.extend_from_slice(&height.to_le_bytes());
    header.push(rotation);
    header.push(color_mode);
    header.extend_from_slice(&(payload.len() as u32).to_le_bytes());

    // CRC32 over header bytes excluding the CRC itself
    let mut hh = Crc32Hasher::new();
    hh.update(&header);
    let header_crc = hh.finalize();
    header.extend_from_slice(&header_crc.to_le_bytes());

    // Compose full file
    let mut file = header;
    file.extend_from_slice(payload);

    let mut ph = Crc32Hasher::new();
    ph.update(payload);
    let payload_crc = ph.finalize();
    file.extend_from_slice(&payload_crc.to_le_bytes());

    file
}

/// Parse a .bin file into header, payload slice, and payload CRC32
pub fn parse_bin_file(data: &[u8]) -> Result<(BinHeader, &[u8], u32)> {
    if data.len() < BIN_HEADER_SIZE + 4 {
        return Err(anyhow!(
            "BIN too small: {} bytes (need at least {} for header + payload CRC)",
            data.len(),
            BIN_HEADER_SIZE + 4
        ));
    }

    let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
    if magic != BIN_MAGIC {
        return Err(anyhow!("Invalid BIN magic: 0x{:08x}", magic));
    }

    let version = data[4];
    let header_len = u16::from_le_bytes([data[5], data[6]]);
    if header_len as usize != BIN_HEADER_SIZE {
        return Err(anyhow!(
            "Unexpected header length: {} (expected {})",
            header_len,
            BIN_HEADER_SIZE
        ));
    }

    let width = u16::from_le_bytes([data[7], data[8]]);
    let height = u16::from_le_bytes([data[9], data[10]]);
    let rotation = data[11];
    let color_mode = data[12];
    let payload_len = u32::from_le_bytes([data[13], data[14], data[15], data[16]]);
    let header_crc32 = u32::from_le_bytes([data[17], data[18], data[19], data[20]]);

    // Validate header CRC32
    let mut hasher = Crc32Hasher::new();
    hasher.update(&data[..BIN_HEADER_SIZE - 4]);
    let calc_header_crc = hasher.finalize();
    if calc_header_crc != header_crc32 {
        return Err(anyhow!(
            "Header CRC mismatch: expected 0x{:08x}, got 0x{:08x}",
            header_crc32,
            calc_header_crc
        ));
    }

    let total_len = BIN_HEADER_SIZE
        .checked_add(payload_len as usize)
        .and_then(|v| v.checked_add(4))
        .ok_or_else(|| anyhow!("BIN length overflow"))?;
    if data.len() < total_len {
        return Err(anyhow!(
            "BIN truncated: have {} bytes, need {} (payload_len={})",
            data.len(),
            total_len,
            payload_len
        ));
    }

    let payload_start = BIN_HEADER_SIZE;
    let payload_end = payload_start + payload_len as usize;
    let payload = &data[payload_start..payload_end];
    let payload_crc32 = u32::from_le_bytes([
        data[payload_end],
        data[payload_end + 1],
        data[payload_end + 2],
        data[payload_end + 3],
    ]);

    Ok((
        BinHeader {
            version,
            header_len,
            width,
            height,
            rotation,
            color_mode,
            payload_len,
            header_crc32,
        },
        payload,
        payload_crc32,
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn build_and_parse_roundtrip() {
        let w = 800u16;
        let h = 480u16;
        let rot = 1u8;
        let color = 1u8;
        let ver = 1u8;
        let payload: Vec<u8> = (0..64u8).collect();

        let file = build_bin_file(&payload, w, h, rot, color, ver);
        let (hdr, pl, crc) = parse_bin_file(&file).expect("parse should succeed");
        assert_eq!(hdr.version, ver);
        assert_eq!(hdr.header_len as usize, BIN_HEADER_SIZE);
        let hw = hdr.width;
        let hh = hdr.height;
        assert_eq!(hw, w);
        assert_eq!(hh, h);
        assert_eq!(hdr.rotation, rot);
        assert_eq!(hdr.color_mode, color);
        assert_eq!(hdr.payload_len as usize, payload.len());

        let mut ph = Crc32Hasher::new();
        ph.update(&payload);
        let want = ph.finalize();
        assert_eq!(crc, want);
        assert_eq!(pl, &payload[..]);
    }
}
