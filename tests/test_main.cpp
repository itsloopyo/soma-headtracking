// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include <iostream>

int RunAimProjectionTests();
int RunBuildProfileTests();
int RunAimRayTests();
int RunHplMathTests();
int RunHeadTrackingTests();
int RunReticleLitmusTests();
int RunCameraFovTests();
int RunCameraPoseTests();
int RunConfigTests();
int RunInjectionWindowTests();
int RunTrackingSettingsTests();

int main() {
    std::cout << "SOMA Head Tracking Tests\n";
    std::cout << "========================\n";

    int failures = 0;
    failures += RunAimProjectionTests();
    failures += RunAimRayTests();
    failures += RunHplMathTests();
    failures += RunHeadTrackingTests();
    failures += RunReticleLitmusTests();
    failures += RunCameraFovTests();
    failures += RunCameraPoseTests();
    failures += RunConfigTests();
    failures += RunInjectionWindowTests();
    failures += RunTrackingSettingsTests();
    failures += RunBuildProfileTests();

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "All tests passed!\n";
        return 0;
    }
    std::cout << failures << " test(s) FAILED\n";
    return 1;
}
