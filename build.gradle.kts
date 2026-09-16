import com.android.build.api.dsl.LibraryExtension
import org.gradle.internal.os.OperatingSystem

plugins {
    alias(libs.plugins.agp.lib) apply false
}

fun String.execute(): String {
    return try {
        val process = ProcessBuilder(*split("\\s".toRegex()).toTypedArray())
            .directory(layout.projectDirectory.asFile)
            .redirectErrorStream(true)
            .start()
        val output = process.inputStream.bufferedReader().use { it.readText() }
        process.waitFor()
        output.trim()
    } catch (_: Exception) {
        ""
    }
}

val gitCommitCount = "git rev-list HEAD --count".execute().toIntOrNull() ?: 1
val gitCommitHash = "git rev-parse --verify --short HEAD".execute().ifEmpty { "unknown" }

// Module metadata, exposed to :loader and :module through rootProject.extra.
val moduleId = "zygisknextsu"
val moduleName = "Zygisk Next Next"
val verName = "v0.8.0"
val verCode = gitCommitCount
val commitHash = gitCommitHash
val minKsuVersion = 10940
val minKsudVersion = 11425
val minMagiskVersion = 26402
val minApatchVersion = 10700

extra.set("moduleId", moduleId)
extra.set("moduleName", moduleName)
extra.set("verName", verName)
extra.set("verCode", verCode)
extra.set("commitHash", commitHash)
extra.set("minKsuVersion", minKsuVersion)
extra.set("minKsudVersion", minKsudVersion)
extra.set("minMagiskVersion", minMagiskVersion)
extra.set("minApatchVersion", minApatchVersion)

// Native build settings shared by every module that compiles C/C++ (:loader and
// :injector), so that the binaries shipped in the same zip never drift apart.
val defaultCFlags = arrayOf(
    "-Wall", "-Wextra",
    "-fno-rtti", "-fno-exceptions",
    "-fno-stack-protector", "-fomit-frame-pointer",
    "-Wno-builtin-macro-redefined", "-D__FILE__=__FILE_NAME__"
)

val releaseCFlags = arrayOf(
    "-Oz", "-flto",
    "-Wno-unused", "-Wno-unused-parameter",
    "-fvisibility=hidden", "-fvisibility-inlines-hidden",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
)

val releaseLinkerFlags = "-Wl,--exclude-libs,ALL -Wl,--gc-sections -Wl,--strip-all"

// ccache is used whenever it is available on PATH, or through -Pccache.path=...
val ccachePath: String? = run {
    val executable = "ccache" + if (OperatingSystem.current().isWindows) ".exe" else ""
    System.getenv("PATH")?.split(File.pathSeparator).orEmpty()
        .map { File(it, executable) }
        .firstOrNull { it.exists() }
        ?.absolutePath
        ?: findProperty("ccache.path") as? String
}

extra.set("defaultCFlags", defaultCFlags)
extra.set("releaseCFlags", releaseCFlags)
extra.set("releaseLinkerFlags", releaseLinkerFlags)
extra.set("ccachePath", ccachePath)

// Android build configuration (used only by this script).
val androidMinSdkVersion = 26
val androidCompileSdkVersion = 37
val androidCompileSdkMinorVersion = 1
val androidBuildToolsVersion = "37.0.0"
val androidCompileNdkVersion = "29.0.13599879"

tasks.register("Delete", Delete::class) {
    delete(layout.buildDirectory)
}

fun Project.configureBaseExtension() {
    extensions.configure<LibraryExtension> {
        namespace = "xyz.baaad.znn"
        compileSdk {
            version = release(androidCompileSdkVersion) {
                minorApiLevel = androidCompileSdkMinorVersion
            }
        }
        defaultConfig {
            minSdk = androidMinSdkVersion
        }
        ndkVersion = androidCompileNdkVersion
        buildToolsVersion = androidBuildToolsVersion
        lint {
            abortOnError = true
        }
    }
}

subprojects {
    plugins.withId("com.android.library") {
        configureBaseExtension()
    }
}
