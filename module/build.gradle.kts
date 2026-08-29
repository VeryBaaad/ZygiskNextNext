import android.databinding.tool.ext.capitalizeUS
import java.security.MessageDigest
import org.apache.tools.ant.filters.ReplaceTokens

import org.apache.tools.ant.filters.FixCrLfFilter

import org.apache.commons.codec.binary.Hex
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.KeyFactory
import java.security.KeyPairGenerator
import java.security.Signature
import java.security.interfaces.EdECPrivateKey
import java.security.interfaces.EdECPublicKey
import java.security.spec.EdECPrivateKeySpec
import java.security.spec.NamedParameterSpec
import java.util.TreeSet

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

android {
    androidResources {
        enable = false
    }
    buildFeatures {
        buildConfig = false
    }
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
            ":webui:buildWebui",
        )
        into(moduleDir)
        from("${rootProject.projectDir}/README.md")
        from("$projectDir/src") {
            exclude("module.prop", "customize.sh", "post-fs-data.sh", "service.sh", "loader-ctl.sh")
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
                "versionCode" to verCode
            )
        }
        from("$projectDir/src") {
            include("customize.sh", "post-fs-data.sh", "service.sh", "loader-ctl.sh")
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
        // AGP stores native artifacts under
        // intermediates/cxx/<cmakeBuildType>/<hash>/obj/<abi>/. The hash is
        // unstable, so locate the obj/ directory at execution time and copy
        // libloader.so -> lib/<abi>/ and injector -> bin/<abi>/ explicitly
        // (Gradle's from()+eachFile flattening proved unreliable here).
        val cmakeBuildType = if (buildTypeLowered == "debug") "Debug" else "RelWithDebInfo"

        doLast {
            val objRoot = project(":loader").layout.buildDirectory
                .dir("intermediates/cxx/$cmakeBuildType").get().asFile
            val hashDir = objRoot.listFiles()?.firstOrNull { it.isDirectory } ?: return@doLast
            val objDir = File(hashDir, "obj")
            if (!objDir.isDirectory) return@doLast

            val dstRoot = moduleDir.get().asFile
            objDir.listFiles()?.forEach { abiDir ->
                if (!abiDir.isDirectory) return@forEach
                val abi = abiDir.name

                File(abiDir, "libloader.so").takeIf { it.isFile }?.let { so ->
                    val libDir = File(dstRoot, "lib/$abi")
                    libDir.mkdirs()
                    so.copyTo(File(libDir, "libloader.so"), overwrite = true)
                }
                File(abiDir, "injector").takeIf { it.isFile }?.let { exe ->
                    val binDir = File(dstRoot, "bin/$abi")
                    binDir.mkdirs()
                    exe.copyTo(File(binDir, "injector"), overwrite = true)
                }
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

    val zipTask = tasks.register<Zip>("zip$variantCapped") {
        group = "module"
        dependsOn(prepareModuleFilesTask)
        archiveFileName.set(zipFileName)
        destinationDirectory.set(layout.buildDirectory.file("outputs/release").get().asFile)
        from(moduleDir)
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
