# Install camflow from a release

Download the release archive that matches the target architecture (`linux-x86_64` or `linux-arm64`), unpack it on the development machine and copy the executable to the target:

```bash
unzip camflow-runtime-linux-arm64-<version>.zip
scp camflow-runtime-linux-arm64-<version>/bin/camflow root@<target>:/usr/local/bin/
ssh root@<target> chmod +x /usr/local/bin/camflow
```

On the target, start the runtime with its camera device, for example:

```bash
camflow --device /dev/video0
```

The archive contains the release documentation in `docs/`.