use photoframe_lib::{
    ColorMode, DitheringResult, photoframe_convert_with_processing, photoframe_dithering_free,
};
use std::slice;

fn main() {
    let path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "tests/data/default_portrait.jpg".to_string());
    let bytes = std::fs::read(&path).expect("read image");

    unsafe {
        // 0 = BW, rotation = 0
        let res: DitheringResult = photoframe_convert_with_processing(
            bytes.as_ptr(),
            bytes.len(),
            0,
            ColorMode::BlackAndWhite.into(),
        );
        if res.success {
            let payload: &[u8] = slice::from_raw_parts(res.data_ptr, res.data_len);
            println!(
                "BW payload len={} dims={}x{}",
                payload.len(),
                res.width,
                res.height
            );
            // Save
            std::fs::create_dir_all("examples/output").ok();
            std::fs::write("examples/output/ffi_bw.pfr1", payload).expect("write");
            // Free
            photoframe_dithering_free(res.data_ptr, res.data_len);
        } else {
            eprintln!("Conversion failed (BW)");
        }

        // 1 = SixColors, rotation = 0
        let res2: DitheringResult = photoframe_convert_with_processing(
            bytes.as_ptr(),
            bytes.len(),
            0,
            ColorMode::SixColors.into(),
        );
        if res2.success {
            let payload: &[u8] = slice::from_raw_parts(res2.data_ptr, res2.data_len);
            println!(
                "6C payload len={} dims={}x{}",
                payload.len(),
                res2.width,
                res2.height
            );
            // If payload includes a 4-byte header, strip it to get raw w*h data
            let expected = (res2.width as usize) * (res2.height as usize);
            let data_to_write: &[u8] = if payload.len() == 4 + expected {
                &payload[4..]
            } else if payload.len() == expected {
                &payload
            } else if payload.len() > expected {
                &payload[payload.len() - expected..]
            } else {
                payload
            };
            std::fs::write("examples/output/ffi_6c.pfr1", data_to_write).expect("write");
            photoframe_dithering_free(res2.data_ptr, res2.data_len);
        } else {
            eprintln!("Conversion failed (6C)");
        }
    }
}
