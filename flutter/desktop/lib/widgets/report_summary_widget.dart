import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:photoframe_flutter/core/models/processor_message.dart';

class ReportSummaryWidget extends StatelessWidget {
  final ProcessorSummary? summary;

  const ReportSummaryWidget({super.key, this.summary});

  @override
  Widget build(BuildContext context) {
    // Debug logging
    debugPrint('=== REPORT SUMMARY WIDGET DEBUG ===');
    debugPrint('Summary: $summary');

    if (summary == null) {
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
    final s = summary;
    if (s == null) {
      return const Text('No summary data available');
    }

    return Column(
      children: [
        _buildSummaryRow('Total files discovered', (s.totalFiles).toString()),
        _buildSummaryRow('Processed', (s.processed).toString()),
        _buildSummaryRow('Paired', (s.paired).toString()),
        _buildSummaryRow('Failed', (s.failed).toString()),
        _buildSummaryRow('Total output images', (s.totalOutputImages).toString()),
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
