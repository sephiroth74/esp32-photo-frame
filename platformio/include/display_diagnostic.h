// Display Diagnostic Tool Header
#ifndef __DISPLAY_DIAGNOSTIC_H__
#define __DISPLAY_DIAGNOSTIC_H__

namespace display_diagnostic {
    void measureVoltage();
    void testSPICommunication();
    void displayColorTestPattern();
    void testRefreshModes();
    void runFullDiagnostic();
}

#endif // __DISPLAY_DIAGNOSTIC_H__