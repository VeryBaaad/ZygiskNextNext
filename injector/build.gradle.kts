plugins {
    alias(libs.plugins.agp.lib)
}

val verCode: Int = rootProject.extra["verCode"] as Int
val verName: String = rootProject.extra["verName"] as String
val commitHash: String = rootProject.extra["commitHash"] as String

@Suppress("UNCHECKED_CAST")
val defaultCFlags: Array<String> = rootProject.extra["defaultCFlags"] as Array<String>

@Suppress("UNCHECKED_CAST")
val releaseFlags: Array<String> = rootProject.extra["releaseCFlags"] as Array<String>
val releaseLinkerFlags: String = rootProject.extra["releaseLinkerFlags"] as String
val ccachePath: String? = rootProject.extra["ccachePath"] as String?

ccachePath?.let {
    println("injector: Use ccache: $it")
}

// :injector only builds an executable; it is never packaged as an Android
// library. The :module script picks the binary up from the CMake output.
android {
    androidResources {
        enable = false
    }
    buildFeatures {
        buildConfig = false
    }

    externalNativeBuild.cmake {
        path("src/CMakeLists.txt")
    }

    defaultConfig {
        externalNativeBuild.cmake {
            arguments += "-DANDROID_STL=c++_static"
            cFlags("-std=c18", *defaultCFlags)
            cppFlags("-std=c++20", *defaultCFlags)
            ccachePath?.let {
                arguments += "-DNDK_CCACHE=$it"
            }
            abiFilters("arm64-v8a", "armeabi-v7a", "x86", "x86_64", "riscv64")
        }
    }

    buildTypes {
        debug {
            externalNativeBuild.cmake {
                arguments += "-DZNN_VERSION=$verName-$verCode-$commitHash-debug"
            }
        }
        release {
            externalNativeBuild.cmake {
                cFlags += releaseFlags
                cppFlags += releaseFlags
                arguments += "-DCMAKE_EXE_LINKER_FLAGS=$releaseLinkerFlags"
                arguments += "-DZNN_VERSION=$verName-$verCode-$commitHash-release"
            }
        }
    }
}
