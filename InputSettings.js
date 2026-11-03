.pragma library

function prepareData(data) {
    let availableLayouts = data.availableLayouts || []
    let layoutNames = data.LayoutList || []
    let variants = data.VariantList || []
    let displayNames = data.DisplayNames || []
    let keyboardLayouts = []

    for (let i = 0; i < layoutNames.length; ++i) {
        let variant = i < variants.length ? variants[i] : ""
        let displayName = i < displayNames.length ? displayNames[i] : ""
        let match = availableLayouts.find(function (item) {
            return item.layout === layoutNames[i] && item.variant === variant
        })
        keyboardLayouts.push({
            layout: layoutNames[i],
            variant: variant,
            displayName: displayName,
            label: displayName || (match ? match.label : layoutNames[i] + "/" + variant)
        })
    }

    let mice = []
    let touchpads = []
    let devices = data.devices || []
    for (let j = 0; j < devices.length; ++j) {
        let device = devices[j]
        let pointer = {
            sysName: device.sysName,
            name: device.name,
            pointerAcceleration: Number(device.pointerAcceleration || 0),
            supportsPointerAcceleration: Boolean(device.supportsPointerAcceleration)
        }
        if (device.touchpad) {
            pointer.tapFingerCount = Number(device.tapFingerCount || 0)
            pointer.tapToClick = Boolean(device.tapToClick)
            pointer.naturalScroll = Boolean(device.naturalScroll)
            pointer.supportsNaturalScroll = Boolean(device.supportsNaturalScroll)
            pointer.scrollFactor = Number(
                device.scrollFactor === undefined ? 1 : device.scrollFactor
            )
            touchpads.push(pointer)
        } else if (device.mouse) {
            pointer.supportsAdaptiveAcceleration = Boolean(
                device.supportsPointerAccelerationProfileAdaptive
            )
            pointer.supportsFlatAcceleration = Boolean(
                device.supportsPointerAccelerationProfileFlat
            )
            pointer.adaptiveAcceleration = Boolean(
                device.pointerAccelerationProfileAdaptive
            )
            mice.push(pointer)
        }
    }

    return {
        keyboardLayouts: keyboardLayouts,
        availableKeyboardLayouts: availableLayouts,
        mouseDevices: mice,
        touchpadDevices: touchpads
    }
}

function addKeyboardLayout(configuredLayouts, selectedLayout) {
    if (!selectedLayout || !selectedLayout.layout || selectedLayout.variant === undefined)
        throw new Error("Choose a keyboard layout from the available list.")

    for (let i = 0; i < configuredLayouts.length; ++i) {
        if (configuredLayouts[i].layout === selectedLayout.layout
                && configuredLayouts[i].variant === selectedLayout.variant)
            throw new Error("That keyboard layout is already configured.")
    }

    let updated = configuredLayouts.slice()
    updated.push({
        layout: selectedLayout.layout,
        variant: selectedLayout.variant,
        displayName: ""
    })
    return updated
}

function removeKeyboardLayout(configuredLayouts, index) {
    if (index < 0 || index >= configuredLayouts.length)
        throw new Error("Choose a configured keyboard layout to remove.")
    return configuredLayouts.filter(function (_layout, itemIndex) {
        return itemIndex !== index
    })
}
