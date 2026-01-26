/// Statistics about color usage in the binary data
#[derive(Debug)]
pub struct ColorStats {
    pub unique_colors: std::collections::HashMap<u8, u32>,
    pub total_pixels: usize,
    #[allow(dead_code)]
    pub most_common_color: Option<(u8, u32)>,
}

impl ColorStats {
    #[allow(dead_code)]
    pub fn analyze(binary_data: &[u8]) -> Self {
        let mut color_counts = std::collections::HashMap::new();

        for &color in binary_data {
            *color_counts.entry(color).or_insert(0) += 1;
        }

        let most_common_color = color_counts
            .iter()
            .max_by_key(|(_, &count)| count)
            .map(|(&color, &count)| (color, count));

        Self {
            unique_colors: color_counts,
            total_pixels: binary_data.len(),
            most_common_color,
        }
    }

    #[allow(dead_code)]
    pub fn unique_color_count(&self) -> usize {
        self.unique_colors.len()
    }

    #[allow(dead_code)]
    pub fn color_usage_percent(&self, color: u8) -> f64 {
        if let Some(&count) = self.unique_colors.get(&color) {
            (count as f64 / self.total_pixels as f64) * 100.0
        } else {
            0.0
        }
    }
}
