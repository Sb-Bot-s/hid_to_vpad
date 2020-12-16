FROM wiiuenv/devkitppc:20200810

COPY --from=wiiuenv/wiiupluginsystem:20200829 /artifacts $DEVKITPRO
COPY --from=wiiuenv/controller_patcher:20201216 /artifacts $DEVKITPRO

WORKDIR project