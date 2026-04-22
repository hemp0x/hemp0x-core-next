#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

HEMP0XD=${HEMP0XD:-$SRCDIR/hemp0xd}
HEMP0XCLI=${HEMP0XCLI:-$SRCDIR/hemp0x-cli}
HEMP0XTX=${HEMP0XTX:-$SRCDIR/hemp0x-tx}

[ ! -x $HEMP0XD ] && echo "$HEMP0XD not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
HEMPVER=($($HEMP0XCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for hemp0xd if --version-string is not set,
# but has different outcomes for hemp0x-cli.
echo "[COPYRIGHT]" > footer.h2m
$HEMP0XD --version | sed -n '1!p' >> footer.h2m

for cmd in $HEMP0XD $HEMP0XCLI $HEMP0XTX; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${HEMPVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${HEMPVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
