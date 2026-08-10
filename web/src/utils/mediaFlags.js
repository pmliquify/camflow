export const MEDIA_ENTITY_FLAG_DEFAULT = 1 << 0;
export const MEDIA_ENTITY_FLAG_CONNECTOR = 1 << 1;

export const MEDIA_PAD_FLAG_SINK = 1 << 0;
export const MEDIA_PAD_FLAG_SOURCE = 1 << 1;
export const MEDIA_PAD_FLAG_MUST_CONNECT = 1 << 2;

export const MEDIA_LINK_FLAG_ENABLED = 1 << 0;
export const MEDIA_LINK_FLAG_IMMUTABLE = 1 << 1;
export const MEDIA_LINK_FLAG_DYNAMIC = 1 << 2;
export const MEDIA_LINK_TYPE_MASK = 0xf0000000;
export const MEDIA_LINK_TYPE_DATA = 0x00000000;
export const MEDIA_LINK_TYPE_INTERFACE = 0x10000000;
export const MEDIA_LINK_TYPE_ANCILLARY = 0x20000000;

const FLAG_DEFINITIONS = {
        entity: {
                flags: [
                        { mask: MEDIA_ENTITY_FLAG_DEFAULT, label: 'default' },
                        { mask: MEDIA_ENTITY_FLAG_CONNECTOR, label: 'connector' }
                ]
        },
        pad: {
                flags: [
                        { mask: MEDIA_PAD_FLAG_SINK, label: 'sink' },
                        { mask: MEDIA_PAD_FLAG_SOURCE, label: 'source' },
                        { mask: MEDIA_PAD_FLAG_MUST_CONNECT, label: 'must connect' }
                ]
        },
        link: {
                flags: [
                        { mask: MEDIA_LINK_FLAG_ENABLED, label: 'enabled' },
                        { mask: MEDIA_LINK_FLAG_IMMUTABLE, label: 'immutable' },
                        { mask: MEDIA_LINK_FLAG_DYNAMIC, label: 'dynamic' }
                ],
                typeMask: MEDIA_LINK_TYPE_MASK,
                types: [
                        { value: MEDIA_LINK_TYPE_DATA, label: 'data link' },
                        { value: MEDIA_LINK_TYPE_INTERFACE, label: 'interface link' },
                        { value: MEDIA_LINK_TYPE_ANCILLARY, label: 'ancillary link' }
                ]
        }
};

export function unsignedMediaFlags(flags) {
        return Number(flags || 0) >>> 0;
}

export function mediaFlagsHex(flags) {
        return `0x${unsignedMediaFlags(flags).toString(16).padStart(8, '0')}`;
}

export function hasMediaFlag(flags, mask) {
        return (unsignedMediaFlags(flags) & mask) !== 0;
}

export function mediaFlagEntries(kind, flags) {
        const value = unsignedMediaFlags(flags);
        const definition = FLAG_DEFINITIONS[kind];
        if (!definition) return [];

        const entries = definition.flags.map((flag) => ({
                label: flag.label,
                set: (value & flag.mask) === flag.mask
        }));
        let knownMask = definition.flags.reduce((mask, flag) => mask | flag.mask, 0) >>> 0;

        if (definition.types) {
                const selectedType = value & definition.typeMask;
                entries.push(...definition.types.map((type) => ({
                        label: type.label,
                        set: selectedType === type.value
                })));
                if (!definition.types.some((type) => type.value === selectedType)) {
                        entries.push({ label: `unknown type ${mediaFlagsHex(selectedType)}`, set: true });
                }
                knownMask = (knownMask | definition.typeMask) >>> 0;
        }

        const unknownFlags = (value & ~knownMask) >>> 0;
        if (unknownFlags !== 0) {
                entries.push({ label: `unknown ${mediaFlagsHex(unknownFlags)}`, set: true });
        }
        return entries;
}
