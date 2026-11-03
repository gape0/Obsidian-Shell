.pragma library

function build(settings) {
    let accent = String(settings.accent)
    let background = String(settings.background)
    let foreground = String(settings.foreground)
    let fontSize = Number(settings.font_size)
    let hexColor = /^#[0-9a-fA-F]{6}$/
    if (!hexColor.test(accent) || !hexColor.test(background) || !hexColor.test(foreground))
        throw new Error("Colors must be six-digit hexadecimal values.")
    if (!Number.isInteger(fontSize) || fontSize < 6 || fontSize > 32)
        throw new Error("Kitty font size must be between 6 and 32.")

    let kitty = [
        "# Managed by Obsidian Settings. Change options in the Obsidian app.",
        "background " + background,
        "foreground " + foreground,
        "selection_background " + accent,
        "cursor " + accent,
        "font_size " + fontSize,
        "enable_audio_bell no",
        "confirm_os_window_close 0",
        ""
    ].join("\n")

    let fish = [
        "# Managed by Obsidian Settings. Change options in the Obsidian app.",
        "function fish_prompt",
        "    set_color '" + accent.replace(/\\/g, "\\\\").replace(/'/g, "\\'") + "'",
        "    printf '%s' (prompt_pwd)",
        "    set_color normal",
        "    printf ' > '",
        "end",
        ""
    ].join("\n")
    return { settings: settings, kitty: kitty, fish: fish }
}
