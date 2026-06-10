# Aroma Beta 26+ rejects plugins built with the old devkitPPC/WUPS heap layout.
# Keep the build image aligned with projects already updated for WUPS 0.9.1+.
FROM ghcr.io/wiiu-env/devkitppc:20260225

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/controller_patcher:20201216 /artifacts $DEVKITPRO

WORKDIR project
