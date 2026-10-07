# bpf

`exec.bpf.o` is the prebuilt BPF object the `exec.bpf.c` for fallback to build.

```sh
clang -O2 -g -target bpf -c exec.bpf.c -o exec.bpf.o
```
with

```shell
$ clang --version

clang version 23.1.1
Target: bpf
Thread model: posix
InstalledDir: /usr/bin
```
