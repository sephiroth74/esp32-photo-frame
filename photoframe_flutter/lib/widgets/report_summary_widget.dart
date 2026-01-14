import 'package:flutter/material.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';

class ReportSummaryWidget extends StatelessWidget {
  final Map<String, dynamic>? summary;
  final Map<String, dynamic>? report;

  const ReportSummaryWidget({
    super.key,
    this.summary,
    this.report,
  });

  @override
  Widget build(BuildContext context) {
    if (summary == null) {
      return const SizedBox.shrink();
    }

    return AppKitGroupBox(
      style: AppKitGroupBoxStyle.roundedScrollBox,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Processing Summary',
            style: TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 12),
          _buildSummaryRow('Total images', summary!['total_images']?.toString() ?? '0'),
          if (summary!['skipped_images'] != null && summary!['skipped_images'] > 0)
            _buildSummaryRow('Skipped (unpaired)', summary!['skipped_images']?.toString() ?? '0'),
          _buildSummaryRow(
            'Images with people',
            '${summary!['images_with_people'] ?? 0} (${_formatPercentage(summary!['people_detection_rate'])})',
          ),
          _buildSummaryRow(
            'Images with pastel tones',
            '${summary!['images_with_pastel'] ?? 0} (${_formatPercentage(summary!['pastel_rate'])})',
          ),
          _buildSummaryRow('Landscape images', summary!['landscape_images']?.toString() ?? '0'),
          _buildSummaryRow('Portrait pairs', summary!['portrait_pairs']?.toString() ?? '0'),
          _buildSummaryRow('Individual portraits', summary!['individual_portraits']?.toString() ?? '0'),

          if (report != null) ...[
            const SizedBox(height: 16),
            const Divider(),
            const SizedBox(height: 16),
            const Text(
              'Detailed Report',
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 12),
            if (report!['landscape_entries'] != null && (report!['landscape_entries'] as List).isNotEmpty)
              _buildReportSection('Landscape Images', report!['landscape_entries'] as List),
            if (report!['portrait_pairs'] != null && (report!['portrait_pairs'] as List).isNotEmpty)
              _buildPortraitPairsSection('Portrait Pairs', report!['portrait_pairs'] as List),
            if (report!['portrait_entries'] != null && (report!['portrait_entries'] as List).isNotEmpty)
              _buildReportSection('Individual Portraits', report!['portrait_entries'] as List),
          ],
        ],
      ),
    );
  }

  Widget _buildSummaryRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label),
          Text(
            value,
            style: const TextStyle(fontWeight: FontWeight.w500),
          ),
        ],
      ),
    );
  }

  Widget _buildReportSection(String title, List entries) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          '$title (${entries.length})',
          style: const TextStyle(
            fontSize: 14,
            fontWeight: FontWeight.w500,
          ),
        ),
        const SizedBox(height: 8),
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: DataTable(
            columns: const [
              DataColumn(label: Text('Input')),
              DataColumn(label: Text('Output')),
              DataColumn(label: Text('Dither')),
              DataColumn(label: Text('Strength')),
              DataColumn(label: Text('Contrast')),
              DataColumn(label: Text('People')),
            ],
            rows: entries.map((entry) => _buildDataRow(entry)).toList(),
          ),
        ),
        const SizedBox(height: 12),
      ],
    );
  }

  Widget _buildPortraitPairsSection(String title, List pairs) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          '$title (${pairs.length})',
          style: const TextStyle(
            fontSize: 14,
            fontWeight: FontWeight.w500,
          ),
        ),
        const SizedBox(height: 8),
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: DataTable(
            columns: const [
              DataColumn(label: Text('Left/Right')),
              DataColumn(label: Text('Combined Output')),
              DataColumn(label: Text('Dither')),
              DataColumn(label: Text('People')),
            ],
            rows: pairs.expand((pair) {
              return [
                DataRow(cells: [
                  DataCell(Text(_truncateFilename(pair['left']['input_filename']))),
                  DataCell(Text(_truncateFilename(pair['combined_output']))),
                  DataCell(Text(_formatDitherMethod(pair['left']['dither_method']))),
                  DataCell(Text(pair['left']['people_detected'] ? '✓' : '✗')),
                ]),
                DataRow(cells: [
                  DataCell(Text(_truncateFilename(pair['right']['input_filename']))),
                  const DataCell(Text('')),
                  DataCell(Text(_formatDitherMethod(pair['right']['dither_method']))),
                  DataCell(Text(pair['right']['people_detected'] ? '✓' : '✗')),
                ]),
              ];
            }).toList(),
          ),
        ),
        const SizedBox(height: 12),
      ],
    );
  }

  DataRow _buildDataRow(Map<String, dynamic> entry) {
    return DataRow(cells: [
      DataCell(Text(_truncateFilename(entry['input_filename']))),
      DataCell(Text(_truncateFilename(entry['output_filename']))),
      DataCell(Text(_formatDitherMethod(entry['dither_method']))),
      DataCell(Text(entry['dither_strength']?.toStringAsFixed(1) ?? '')),
      DataCell(Text(entry['contrast']?.toStringAsFixed(1) ?? '')),
      DataCell(Text(entry['people_detected'] == true
        ? '✓ (${entry['people_count']})'
        : '✗')),
    ]);
  }

  String _formatDitherMethod(dynamic method) {
    if (method is String) {
      return method.replaceAll('_', '-');
    } else if (method is Map) {
      // Handle enum serialization from Rust
      return method.toString();
    }
    return '';
  }

  String _truncateFilename(String? filename) {
    if (filename == null) return '';
    if (filename.length <= 20) return filename;
    return '${filename.substring(0, 17)}...';
  }

  String _formatPercentage(dynamic value) {
    if (value == null) return '0%';
    if (value is num) {
      return '${value.toStringAsFixed(1)}%';
    }
    return '0%';
  }
}