import '../../models/processing_models.dart';

String labelForDithering(DitheringMethod method) {
  switch (method) {
    case DitheringMethod.floydSteinberg:
      return 'Floyd-Steinberg';
    case DitheringMethod.atkinson:
      return 'Atkinson';
    case DitheringMethod.stucki:
      return 'Stucki';
    case DitheringMethod.jarvisJudiceNinke:
      return 'Jarvis-Judice-Ninke';
    case DitheringMethod.ordered:
      return 'Ordered';
  }
}
