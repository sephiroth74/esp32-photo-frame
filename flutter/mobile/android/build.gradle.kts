// The Flutter Gradle Plugin registers project-level repositories for the engine artifacts,
// so dependency resolution stays project-scoped rather than centralized in settings.gradle.kts.
allprojects {
    repositories {
        google()
        mavenCentral()
    }
}

val newBuildDir: Directory =
    rootProject.layout.buildDirectory
        .dir("../../build")
        .get()
rootProject.layout.buildDirectory.value(newBuildDir)

subprojects {
    val newSubprojectBuildDir: Directory = newBuildDir.dir(project.name)
    project.layout.buildDirectory.value(newSubprojectBuildDir)
}
subprojects {
    project.evaluationDependsOn(":app")
}

// Some Flutter plugins still pin an old compileSdk (app_links 31, app_settings 33, share_plus 33).
// AGP 9 rejects that because their AndroidX dependencies require at least 34, so every plugin is
// aligned with the app's compileSdk until those plugins are upgraded.
subprojects {
    // :app is already evaluated by the evaluationDependsOn above, and it is the reference anyway.
    if (path == ":app") return@subprojects
    afterEvaluate {
        val appCompileSdk =
            (project(":app").extensions.getByName("android") as com.android.build.gradle.BaseExtension)
                .compileSdkVersion
        val pluginAndroid = extensions.findByName("android") as? com.android.build.gradle.BaseExtension
        if (appCompileSdk != null && pluginAndroid != null && pluginAndroid.compileSdkVersion != appCompileSdk) {
            logger.lifecycle("Overriding compileSdk of $path from ${pluginAndroid.compileSdkVersion} to $appCompileSdk")
            pluginAndroid.compileSdkVersion(appCompileSdk)
        }
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.layout.buildDirectory)
}
