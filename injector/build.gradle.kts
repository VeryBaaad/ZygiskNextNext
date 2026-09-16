import org.gradle.internal.os.OperatingSystem

val verCode: Int = rootProject.extra["verCode"] as Int
val verName: String = rootProject.extra["verName"] as String
val commitHash: String = rootProject.extra["commitHash"] as String
val minSdk: Int = rootProject.extra["androidMinSdkVersion"] as Int
val ndkVersion: String = rootProject.extra["androidNdkVersion"] as String

val cargoVersion: String = verName.removePrefix("v")

val variants = listOf("debug", "release")

val rustAbis = linkedMapOf(
    "arm64-v8a" to "aarch64-linux-android",
    "armeabi-v7a" to "armv7-linux-androideabi",
    "x86" to "i686-linux-android",
    "x86_64" to "x86_64-linux-android",
    "riscv64" to "riscv64-linux-android",
)

val riscv64Triple = "riscv64-linux-android"

fun sdkDirectory(): File? {
    for (variable in listOf("ANDROID_SDK_ROOT", "ANDROID_HOME")) {
        val configured = System.getenv(variable) ?: continue
        val directory = File(configured)
        if (directory.isDirectory) return directory
    }
    val localProperties = rootProject.file("local.properties")
    if (localProperties.isFile) {
        val configured = localProperties.readLines()
            .firstOrNull { it.startsWith("sdk.dir=") }
            ?.removePrefix("sdk.dir=")
            ?.trim()
        if (!configured.isNullOrEmpty()) return File(configured)
    }
    return null
}

fun ndkDirectory(): File? {
    for (variable in listOf("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT")) {
        val configured = System.getenv(variable) ?: continue
        val directory = File(configured)
        if (directory.isDirectory) return directory
    }
    val sdk = sdkDirectory() ?: return null
    val versioned = File(sdk, "ndk/$ndkVersion")
    if (versioned.isDirectory) return versioned
    return File(sdk, "ndk").listFiles()?.maxByOrNull { it.name }
}

fun riscv64Linker(): File? {
    val ndk = ndkDirectory() ?: return null
    val hostTag = when {
        OperatingSystem.current().isLinux -> "linux-x86_64"
        OperatingSystem.current().isMacOsX -> "darwin-x86_64"
        else -> "windows-x86_64"
    }
    val binaries = File(ndk, "toolchains/llvm/prebuilt/$hostTag/bin")
    return binaries.listFiles { file ->
        file.name.startsWith("riscv64-linux-android") && file.name.endsWith("-clang")
    }?.maxByOrNull { it.name }
}

val syncCargoVersion = tasks.register("syncCargoVersion") {
    group = "rust"
    val manifest = file("Cargo.toml")
    doLast {
        val contents = manifest.readText()
        val versioned = Regex("(?m)^version\\s*=\\s*\"[^\"]*\"")
            .replaceFirst(contents, "version = \"$cargoVersion\"")
        if (versioned != contents) manifest.writeText(versioned)
    }
}

variants.forEach { variant ->
    val variantCapped = variant.replaceFirstChar { it.uppercase() }
    val profile = if (variant == "debug") "debug" else "release"
    val profileArguments = if (profile == "release") listOf("--release") else emptyList()
    val reportedVersion = "$verName-$verCode-$commitHash-$variant"
    val outputDirectory = layout.buildDirectory.dir("rust/$variant")

    val builds = rustAbis.map { (abi, triple) ->
        tasks.register<Exec>("cargoBuild$variantCapped${abi.replace("-", "")}") {
            group = "rust"
            description = "Builds the Rust injector for $abi ($triple)."
            dependsOn(syncCargoVersion)
            workingDir(projectDir)
            environment("ZNN_VERSION", reportedVersion)

            if (triple == riscv64Triple) {
                val linker = riscv64Linker()
                if (linker == null) {
                    doFirst {
                        throw GradleException(
                            "cannot build $abi: the NDK has no riscv64 clang " +
                                "(set ANDROID_NDK_HOME to an NDK r27 or newer)"
                        )
                    }
                } else {
                    environment("CARGO_TARGET_RISCV64_LINUX_ANDROID_LINKER", linker.absolutePath)
                    environment("CC_riscv64_linux_android", linker.absolutePath)
                    environment(
                        "AR_riscv64_linux_android",
                        File(linker.parentFile, "llvm-ar").absolutePath
                    )
                }
                commandLine(
                    listOf(
                        "cargo", "+nightly", "build",
                        "-Z", "build-std=std,panic_abort",
                        "--target", triple
                    ) + profileArguments
                )
            } else {
                commandLine(
                    listOf(
                        "cargo", "ndk",
                        "-t", abi,
                        "-P", minSdk.toString(),
                        "build"
                    ) + profileArguments
                )
            }

            doLast {
                val produced = File(projectDir, "target/$triple/$profile/injector")
                if (!produced.isFile) {
                    throw GradleException("cargo produced no binary at $produced")
                }
                val destination = outputDirectory.get().asFile.resolve("$abi/injector")
                destination.parentFile.mkdirs()
                produced.copyTo(destination, overwrite = true)
            }
        }
    }

    val cargoBuild = tasks.register("cargoBuild$variantCapped") {
        group = "rust"
        description = "Builds the Rust injector for every shipped ABI."
        dependsOn(builds)
    }

    tasks.register("assemble$variantCapped") {
        group = "rust"
        description = "Assembles the Rust injector for the $variant build type."
        dependsOn(cargoBuild)
    }
}
