import android.databinding.tool.ext.capitalizeUS
import java.security.MessageDigest
import org.apache.commons.codec.binary.Hex
import org.apache.tools.ant.filters.FixCrLfFilter
import org.apache.tools.ant.filters.ReplaceTokens

plugins {
    alias(libs.plugins.agp.lib)
}

val moduleId: String = rootProject.extra["moduleId"] as String
val moduleName: String = rootProject.extra["moduleName"] as String
val verCode: Int = rootProject.extra["verCode"] as Int
val verName: String = rootProject.extra["verName"] as String
val minKsuVersion: Int = rootProject.extra["minKsuVersion"] as Int
val minKsudVersion: Int = rootProject.extra["minKsudVersion"] as Int
val minMagiskVersion: Int = rootProject.extra["minMagiskVersion"] as Int
val minApatchVersion: Int = rootProject.extra["minApatchVersion"] as Int
val commitHash: String = rootProject.extra["commitHash"] as String
val updateUrl: String = rootProject.extra["updateUrl"] as String

android {
    androidResources {
        enable = false
    }
    buildFeatures {
        buildConfig = false
    }
}

val xzMinEntrySize = 1024L

fun runProcess(command: List<String>, workingDirectory: File? = null): Pair<Int, String> {
    val process = ProcessBuilder(command)
        .apply { if (workingDirectory != null) directory(workingDirectory) }
        .redirectErrorStream(true)
        .start()
    val output = process.inputStream.bufferedReader().use { it.readText() }
    return process.waitFor() to output
}

fun resolveSevenZip(probeRoot: File): String {
    val candidates = listOfNotNull(
        findProperty("sevenZip.path") as? String,
        "7zz",
        "7z",
        "7za",
    )
    val probeDir = File(probeRoot, "probe").apply { deleteRecursively(); mkdirs() }
    File(probeDir, "probe.txt").writeText("z".repeat(4096))
    try {
        for (candidate in candidates) {
            File(probeDir, "probe.zip").delete()
            try {
                val add = runProcess(
                    listOf(candidate, "a", "-tzip", "-mm=XZ", "probe.zip", "probe.txt"), probeDir
                )
                if (add.first != 0) continue
                val listing = runProcess(listOf(candidate, "l", "-slt", "probe.zip"), probeDir)
                if (listing.first == 0 && "Method = xz" in listing.second) return candidate
            } catch (_: Exception) {
            }
        }
    } finally {
        probeDir.deleteRecursively()
    }
    throw GradleException(
        "No 7-Zip found"
    )
}

androidComponents.onVariants { variant ->
    val variantLowered = variant.name.lowercase()
    val variantCapped = variant.name.capitalizeUS()
    val buildTypeLowered = variant.buildType?.lowercase()

    val moduleDir = layout.buildDirectory.dir("outputs/module/$variantLowered")
    val zipFileName = "$moduleName-$verName-$verCode-$commitHash-$buildTypeLowered.zip".replace(' ', '-')

    val prepareModuleFilesTask = tasks.register<Sync>("prepareModuleFiles$variantCapped") {
        group = "module"
        dependsOn(
            ":loader:assemble$variantCapped",
            ":injector:assemble$variantCapped",
            ":webui:buildWebui",
        )
        into(moduleDir)
        from("${rootProject.projectDir}/README.md")
        from("$projectDir/src") {
            exclude("module.prop", "customize.sh", "post-fs-data.sh")
            filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
        }
        from(rootProject.file("module/webroot")) {
            into("webroot")
        }
        from("$projectDir/src") {
            include("module.prop")
            expand(
                "moduleId" to moduleId,
                "moduleName" to moduleName,
                "versionName" to "$verName ($verCode-$commitHash-$variantLowered)",
                "versionCode" to verCode,
                "updateUrl" to updateUrl
            )
        }
        from("$projectDir/src") {
            include("customize.sh", "post-fs-data.sh")
            val tokens = mapOf(
                "DEBUG" to if (buildTypeLowered == "debug") "true" else "false",
                "MIN_KSU_VERSION" to "$minKsuVersion",
                "MIN_KSUD_VERSION" to "$minKsudVersion",
                "MIN_MAGISK_VERSION" to "$minMagiskVersion",
                "MIN_APATCH_VERSION" to "$minApatchVersion",
            )
            filter<ReplaceTokens>("tokens" to tokens)
            filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
        }

        val cmakeBuildType = if (buildTypeLowered == "debug") "Debug" else "RelWithDebInfo"

        doLast {
            val dstRoot = moduleDir.get().asFile

            // CMake output directory of the loader, holding one subdirectory
            // per ABI (e.g. <obj>/<abi>/libloader.so).
            fun cmakeObjDir(projectPath: String): File? {
                val cxxDir = project(projectPath).layout.buildDirectory
                    .dir("intermediates/cxx/$cmakeBuildType").get().asFile
                val hashDir = cxxDir.listFiles()
                    ?.filter { it.isDirectory && File(it, "obj").isDirectory }
                    ?.maxByOrNull { it.lastModified() }
                    ?: return null
                return File(hashDir, "obj")
            }

            fun collectArtifacts(projectPath: String, artifactName: String, destOf: (String) -> File) {
                cmakeObjDir(projectPath)?.listFiles()?.forEach { abiDir ->
                    val artifact = File(abiDir, artifactName)
                    if (!abiDir.isDirectory || !artifact.isFile) return@forEach
                    val dest = destOf(abiDir.name)
                    dest.parentFile.mkdirs()
                    artifact.copyTo(dest, overwrite = true)
                }
            }

            collectArtifacts(":loader", "libloader.so") { abi -> File(dstRoot, "lib/$abi/libloader.so") }

            // :injector is a Rust crate; its Gradle build leaves one binary per
            // ABI under <injector>/build/rust/<variant>/<abi>/injector.
            val injectorVariants = project(":injector").layout.buildDirectory
                .dir("rust/$variantLowered").get().asFile
            injectorVariants.listFiles()?.forEach { abiDir ->
                val artifact = File(abiDir, "injector")
                if (!abiDir.isDirectory || !artifact.isFile) return@forEach
                val destination = File(dstRoot, "bin/${abiDir.name}/injector")
                destination.parentFile.mkdirs()
                artifact.copyTo(destination, overwrite = true)
            }
        }

        doLast {
            fileTree(moduleDir).visit {
                if (isDirectory) return@visit
                val md = MessageDigest.getInstance("SHA-256")
                file.forEachBlock(4096) { bytes, size ->
                    md.update(bytes, 0, size)
                }
                file(file.path + ".sha256").writeText(Hex.encodeHexString(md.digest()))
            }
        }
    }

    val releaseDir = layout.buildDirectory.dir("outputs/release").get().asFile
    val archiveFile = File(releaseDir, zipFileName)

    val zipTask = tasks.register("zip$variantCapped") {
        group = "module"
        description = "Packs the $variantLowered module into $zipFileName with XZ-compressed entries."
        dependsOn(prepareModuleFilesTask)
        inputs.dir(moduleDir)
        outputs.file(archiveFile)

        doLast {
            val sevenZip = resolveSevenZip(layout.buildDirectory.dir("sevenzip").get().asFile)
            val root = moduleDir.get().asFile

            releaseDir.mkdirs()
            archiveFile.delete()

            val entries = fileTree(root).files
                .filter { it.isFile }
                .map { it.relativeTo(root).invariantSeparatorsPath }
                .sorted()
            val (deflated, xz) = entries.partition { File(root, it).length() < xzMinEntrySize }

            fun append(method: String, members: List<String>) {
                if (members.isEmpty()) return
                val (status, output) = runProcess(
                    listOf(
                        sevenZip, "a", "-tzip", "-mx=9", "-mm=$method", "-bso0", "-bsp0",
                        archiveFile.absolutePath,
                    ) + members,
                    root,
                )
                if (status != 0) {
                    throw GradleException("7-Zip failed: $method ($status):\n$output")
                }
            }

            append("Deflate", deflated)
            append("XZ", xz)

            logger.lifecycle(
                "$zipFileName: ${archiveFile.length()} bytes " +
                    "(${xz.size} xz + ${deflated.size} deflate entries)"
            )
        }
    }

    val pushTask = tasks.register<Exec>("push$variantCapped") {
        group = "module"
        dependsOn(zipTask)
        commandLine("adb", "push", zipTask.get().outputs.files.singleFile.path, "/data/local/tmp")
    }

    val installKsuTask = tasks.register("installKsu$variantCapped") {
        group = "module"
        dependsOn(pushTask)
        doLast {
            fun run(vararg cmd: String) {
                ProcessBuilder(*cmd).redirectErrorStream(true).start().waitFor()
            }
            run(
                "adb", "shell", "echo",
                "/data/adb/ksud module install /data/local/tmp/$zipFileName",
                "> /data/local/tmp/install.sh"
            )
            run("adb", "shell", "chmod", "755", "/data/local/tmp/install.sh")
            run("adb", "shell", "su", "-c", "/data/local/tmp/install.sh")
        }
    }

    val installMagiskTask = tasks.register<Exec>("installMagisk$variantCapped") {
        group = "module"
        dependsOn(pushTask)
        commandLine("adb", "shell", "su", "-M", "-c", "magisk --install-module /data/local/tmp/$zipFileName")
    }

    tasks.register<Exec>("installKsuAndReboot$variantCapped") {
        group = "module"
        dependsOn(installKsuTask)
        commandLine("adb", "reboot")
    }

    tasks.register<Exec>("installMagiskAndReboot$variantCapped") {
        group = "module"
        dependsOn(installMagiskTask)
        commandLine("adb", "reboot")
    }
}
