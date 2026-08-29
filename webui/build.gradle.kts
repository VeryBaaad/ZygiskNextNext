plugins {
    base
}

val moduleId: String = rootProject.extra["moduleId"] as String
val moduleName: String = rootProject.extra["moduleName"] as String
val verName: String = rootProject.extra["verName"] as String
val commitHash: String = rootProject.extra["commitHash"] as String

val webrootDir: File = rootProject.file("module/webroot")

val npmInstall = tasks.register<Exec>("npmInstall") {
    group = "webui"
    description = "Install the WebUI npm dependencies."
    workingDir(projectDir)
    inputs.file("package.json")
    inputs.file("package-lock.json")
    outputs.dir("node_modules")
    val useCi = file("package-lock.json").exists()
    commandLine("npm", if (useCi) "ci" else "install", "--no-audit", "--no-fund")
}

val buildWebui = tasks.register<Exec>("buildWebui") {
    group = "webui"
    description = "Build the WebUI into module/webroot/."
    dependsOn(npmInstall)
    inputs.file("index.html")
    inputs.file("package.json")
    inputs.file("tsconfig.json")
    inputs.file("vite.config.ts")
    inputs.dir("src")
    outputs.dir(webrootDir)
    workingDir(projectDir)
    environment("ZNN_MODULE_ID", moduleId)
    environment("ZNN_MODULE_NAME", moduleName)
    environment("ZNN_VER_NAME", verName)
    environment("ZNN_COMMIT_HASH", commitHash)
    commandLine("npm", "run", "build")
}

tasks.named("assemble") {
    dependsOn(buildWebui)
}
