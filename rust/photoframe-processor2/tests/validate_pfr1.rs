use photoframe_lib::{BIN_HEADER_SIZE, BIN_MAGIC, build_bin_file, validate_bin_file};

#[test]
fn validate_pfr1_ok() {
    let width = 16u16;
    let height = 10u16;
    let rotation = 1u8;
    let color_mode = 1u8;
    let version = 1u8;

    let payload_len = width as usize * height as usize;
    let payload: Vec<u8> = (0..payload_len).map(|i| (i % 255) as u8).collect();

    let file = build_bin_file(&payload, width, height, rotation, color_mode, version);
    let validation = validate_bin_file(&file).expect("validation should succeed");

    let header = validation.header;
    let version_read = header.version;
    let header_len = header.header_len;
    let width_read = header.width;
    let height_read = header.height;
    let rotation_read = header.rotation;
    let color_mode_read = header.color_mode;
    let payload_len_read = header.payload_len;

    assert_eq!(version_read, version);
    assert_eq!(header_len as usize, BIN_HEADER_SIZE);
    assert_eq!(width_read, width);
    assert_eq!(height_read, height);
    assert_eq!(rotation_read, rotation);
    assert_eq!(color_mode_read, color_mode);
    assert_eq!(payload_len_read as usize, payload_len);
    assert_eq!(validation.payload, payload.as_slice());
}

#[test]
fn validate_pfr1_bad_payload_crc() {
    let width = 8u16;
    let height = 8u16;
    let payload_len = width as usize * height as usize;
    let payload: Vec<u8> = vec![0x42; payload_len];

    let mut file = build_bin_file(&payload, width, height, 0, 0, 1);

    // Corrupt a payload byte to break CRC
    let payload_start = BIN_HEADER_SIZE;
    file[payload_start] ^= 0xFF;

    let err = validate_bin_file(&file).expect_err("validation should fail");
    assert!(err.to_string().contains("Payload CRC mismatch"));
}

#[test]
fn validate_pfr1_bad_magic() {
    let width = 4u16;
    let height = 4u16;
    let payload_len = width as usize * height as usize;
    let payload: Vec<u8> = vec![0x00; payload_len];

    let mut file = build_bin_file(&payload, width, height, 0, 1, 1);

    // Corrupt magic
    let bad_magic = (BIN_MAGIC ^ 0xFFFF_FFFF).to_le_bytes();
    file[0..4].copy_from_slice(&bad_magic);

    let err = validate_bin_file(&file).expect_err("validation should fail");
    assert!(err.to_string().contains("Invalid BIN magic"));
}
