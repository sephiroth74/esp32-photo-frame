import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:photoframe_flutter/core/models/processor_report.dart';

class ReportSummaryWidget extends StatelessWidget {
  final ProcessorSummary? summary;
  final ProcessorReport? report;

  const ReportSummaryWidget({super.key, this.summary, this.report});

  @override
  Widget build(BuildContext context) {
    // Debug logging
    debugPrint('=== REPORT SUMMARY WIDGET DEBUG ===');
    debugPrint('Summary: $summary');
    if (report != null) {
      debugPrint('Report summary: ${report!.summary}');
      debugPrint('Processed images count: ${report!.processed_images.length}');
      if (report!.processed_images.isNotEmpty) {
        debugPrint('First image: ${report!.processed_images.first}');
      }
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
        ],
      ),
    );
  }

  Widget _buildSummaryFromReport() {
    final s = report?.summary ?? summary;
    if (s == null) {
      return const Text('No summary data available');
    }

    return Column(
      children: [
        _buildSummaryRow('Total files discovered', (s.total_files_discovered ?? 0).toString()),
        _buildSummaryRow('Invalid files', (s.invalid_files ?? 0).toString()),
        _buildSummaryRow('Unpaired images', (s.unpaired_images ?? 0).toString()),
        _buildSummaryRow('Images processed', (s.images_processed ?? 0).toString()),
        _buildSummaryRow('Image pairs created', (s.image_pairs_created ?? 0).toString()),
        _buildSummaryRow('Total output images', (s.total_output_images ?? 0).toString()),
        _buildSummaryRow('Failed images', (s.failed_images ?? 0).toString()),
      ],
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
}
