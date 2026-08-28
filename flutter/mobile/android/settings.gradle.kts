pluginManagement {
    val flutterSdkPath = run {
        val properties = java.util.Properties()
        file("local.properties").inputStream().use { properties.load(it) }
        val flutterSdkPath = properties.getProperty("flutter.sdk")
        require(flutterSdkPath != null) { "flutter.sdk not set in local.properties" }
        flutterSdkPath
    }

    includeBuild("$flutterSdkPath/packages/flutter_tools/gradle")

    // Gradle doesn't expose version catalog accessors in a settings script, so versions are read
    // straight out of the catalog to keep gradle/libs.versions.toml the single source of truth.
    val catalog = file("gradle/libs.versions.toml").readText()
    fun catalogVersion(name: String) =
        Regex("^\\s*$name\\s*=\\s*\"([^\"]+)\"", RegexOption.MULTILINE)
            .find(catalog)
            ?.groupValues?.get(1)
            ?: error("$name version not found in gradle/libs.versions.toml")

    plugins {
        id("com.android.application") version catalogVersion("agp")
        id("org.jetbrains.kotlin.android") version catalogVersion("kotlin")
    }

    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

plugins {
    id("dev.flutter.flutter-plugin-loader")
    id("com.android.application") apply false
    id("org.jetbrains.kotlin.android") apply false
}

include(":app")
