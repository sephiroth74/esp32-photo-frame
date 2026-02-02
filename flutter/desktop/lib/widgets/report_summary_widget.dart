import 'package:flutter/material.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';

class ReportSummaryWidget extends StatelessWidget {
  final Map<String, dynamic>? summary;
  final Map<String, dynamic>? report;

  const ReportSummaryWidget({super.key, this.summary, this.report});

  @override
  Widget build(BuildContext context) {
    // Debug logging
    debugPrint('=== REPORT SUMMARY WIDGET DEBUG ===');
    debugPrint('Summary: $summary');
    debugPrint('Report keys: ${report?.keys.toList()}');
    if (report != null && report!['processed_images'] != null) {
      debugPrint('Processed images count: ${(report!['processed_images'] as List).length}');
      if ((report!['processed_images'] as List).isNotEmpty) {
        debugPrint('First image: ${(report!['processed_images'] as List)[0]}');
      }
    }
    if (report != null && report!['summary'] != null) {
      debugPrint('Report summary: ${report!['summary']}');
    }

    if (summary == null && report == null) {
      return const SizedBox.shrink();
    }

    return AppKitGroupBox(
      style: AppKitGroupBoxStyle.roundedScrollBox,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text('Processing Summary', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
          const SizedBox(height: 12),
          _buildSummaryFromReport(),
          if (report != null && report!['processed_images'] != null && (report!['processed_images'] as List).isNotEmpty) ...[
            const SizedBox(height: 16),
            const Divider(),
            const SizedBox(height: 16),
            const Text('Processed Images', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w500)),
            const SizedBox(height: 8),
            _buildProcessedImagesTable(report!['processed_images'] as List),
          ],
          if (report != null && report!['paired_images'] != null && (report!['paired_images'] as List).isNotEmpty) ...[
            const SizedBox(height: 16),
            const Divider(),
            const SizedBox(height: 16),
            const Text('Paired Images', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w500)),
            const SizedBox(height: 8),
            _buildPairedImagesTable(report!['paired_images'] as List),
          ],
        ],
      ),
    );
  }

  Widget _buildSummaryFromReport() {
    // Try to use report summary first (new format)
    if (report != null && report!['summary'] != null) {
      final s = report!['summary'] as Map<String, dynamic>;
      return Column(
        children: [
          _buildSummaryRow('Total files discovered', s['total_files_discovered']?.toString() ?? '0'),
          _buildSummaryRow('Invalid files', s['invalid_files']?.toString() ?? '0'),
          _buildSummaryRow('Unpaired images', s['unpaired_images']?.toString() ?? '0'),
          _buildSummaryRow('Images processed', s['images_processed']?.toString() ?? '0'),
          _buildSummaryRow('Image pairs created', s['image_pairs_created']?.toString() ?? '0'),
          _buildSummaryRow('Total output images', s['total_output_images']?.toString() ?? '0'),
          _buildSummaryRow('Failed images', s['failed_images']?.toString() ?? '0'),
        ],
      );
    }

    // Fallback to legacy summary format
    if (summary != null) {
      return Column(
        children: [
          _buildSummaryRow('Total images', summary!['total_images']?.toString() ?? '0'),
          if (summary!['skipped_images'] != null && summary!['skipped_images'] > 0)
            _buildSummaryRow('Skipped (unpaired)', summary!['skipped_images']?.toString() ?? '0'),
          _buildSummaryRow('Images with people', '${summary!['images_with_people'] ?? 0} (${_formatPercentage(summary!['people_detection_rate'])})'),
          _buildSummaryRow('Images with pastel tones', '${summary!['images_with_pastel'] ?? 0} (${_formatPercentage(summary!['pastel_rate'])})'),
          _buildSummaryRow('Landscape images', summary!['landscape_images']?.toString() ?? '0'),
          _buildSummaryRow('Portrait pairs', summary!['portrait_pairs']?.toString() ?? '0'),
          _buildSummaryRow('Individual portraits', summary!['individual_portraits']?.toString() ?? '0'),
        ],
      );
    }

    return const Text('No summary data available');
  }

  Widget _buildProcessedImagesTable(List images) {
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: DataTable(
        columns: const [
          DataColumn(label: Text('Source')),
          DataColumn(label: Text('Output')),
          DataColumn(label: Text('Size')),
          DataColumn(label: Text('Faces')),
          DataColumn(label: Text('Total (ms)')),
          DataColumn(label: Text('Detect (ms)')),
        ],
        rows: images.map((image) {
          return DataRow(
            cells: [
              DataCell(Text(_truncateFilename(image['source']))),
              DataCell(Text(_truncateFilename(image['output']))),
              DataCell(Text(image['size'] ?? '')),
              DataCell(Text((image['faces'] ?? 0).toString())),
              DataCell(Text((image['total_ms'] ?? 0).toString())),
              DataCell(Text((image['detect_ms'] ?? 0).toString())),
            ],
          );
        }).toList(),
      ),
    );
  }

  Widget _buildPairedImagesTable(List pairs) {
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: DataTable(
        columns: const [
          DataColumn(label: Text('Source')),
          DataColumn(label: Text('Output')),
          DataColumn(label: Text('Size')),
          DataColumn(label: Text('Faces')),
          DataColumn(label: Text('Total (ms)')),
          DataColumn(label: Text('Detect (ms)')),
        ],
        rows: pairs.map((image) {
          return DataRow(
            cells: [
              DataCell(Text(_truncateFilename(image['source']))),
              DataCell(Text(_truncateFilename(image['output']))),
              DataCell(Text(image['size'] ?? '')),
              DataCell(Text((image['faces'] ?? 0).toString())),
              DataCell(Text((image['total_ms'] ?? 0).toString())),
              DataCell(Text((image['detect_ms'] ?? 0).toString())),
            ],
          );
        }).toList(),
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
          Text(value, style: const TextStyle(fontWeight: FontWeight.w500)),
        ],
      ),
    );
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
