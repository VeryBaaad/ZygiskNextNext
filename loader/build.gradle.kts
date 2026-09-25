plugins {
    alias(libs.plugins.agp.lib)
}

@Suppress("UNCHECKED_CAST")
val defaultCFlags: Array<String> = rootProject.extra["defaultCFlags"] as Array<String>

@Suppress("UNCHECKED_CAST")
val releaseFlags: Array<String> = rootProject.extra["releaseCFlags"] as Array<String>
val releaseLinkerFlags: String = rootProject.extra["releaseLinkerFlags"] as String
val ccachePath: String? = rootProject.extra["ccachePath"] as String?

val libcxxVersion: String = rootProject.extra["androidNdkVersion"] as String

ccachePath?.let {
    println("loader: Use ccache: $it")
}

android {
    androidResources {
        enable = false
    }
    buildFeatures {
        buildConfig = false
        prefab = true
    }

    externalNativeBuild.cmake {
        path("src/CMakeLists.txt")
    }

    defaultConfig {
        externalNativeBuild.cmake {
            arguments += "-DANDROID_STL=none"
            arguments += "-DLSPLT_STANDALONE=ON"
            cFlags("-std=c18", *defaultCFlags)
            cppFlags("-std=c++20", *defaultCFlags)
            ccachePath?.let {
                arguments += "-DNDK_CCACHE=$it"
            }
            abiFilters("arm64-v8a", "armeabi-v7a", "x86", "x86_64", "riscv64")
        }
    }

    buildTypes {
        release {
            externalNativeBuild.cmake {
                cFlags += releaseFlags
                cppFlags += releaseFlags
                arguments += "-DCMAKE_SHARED_LINKER_FLAGS=$releaseLinkerFlags"
                arguments += "-DCMAKE_EXE_LINKER_FLAGS=$releaseLinkerFlags"
            }
        }
    }
}

dependencies {
    implementation("org.lsposed.libcxx:libcxx:$libcxxVersion")
}
